#include "sim_packet.h"
#include <iostream>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <algorithm>

static volatile bool running = true;
void handle_signal(int) { running = false; }

// PID controller
struct PID {
    double kp, ki, kd;
    double integral = 0, prev_error = 0;
    double output_min, output_max;

    double compute(double setpoint, double actual, double dt) {
        double error = setpoint - actual;
        integral += error * dt;
        double derivative = (dt > 0) ? (error - prev_error) / dt : 0;
        prev_error = error;
        double out = kp * error + ki * integral + kd * derivative;
        return std::clamp(out, output_min, output_max);
    }
};

enum class ChamberState {
    IDLE, PUMP_DOWN, GAS_STABILIZE, PROCESS, PURGE, VENT, FAULT
};

const char* state_name(ChamberState s) {
    switch (s) {
        case ChamberState::IDLE:           return "IDLE";
        case ChamberState::PUMP_DOWN:      return "PUMP_DOWN";
        case ChamberState::GAS_STABILIZE:  return "GAS_STABILIZE";
        case ChamberState::PROCESS:        return "PROCESS";
        case ChamberState::PURGE:          return "PURGE";
        case ChamberState::VENT:           return "VENT";
        case ChamberState::FAULT:          return "FAULT";
        default: return "UNKNOWN";
    }
}

int main() {
    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    const char* sim_host = std::getenv("SIM_HOST") ?: "127.0.0.1";
    int sim_port = std::atoi(std::getenv("SIM_PORT") ?: "5555");
    int hmi_port = std::atoi(std::getenv("HMI_PORT") ?: "8080");

    std::cout << "[controller] Connecting to simulator at " << sim_host << ":" << sim_port << std::endl;

    // Resolve simulator host
    struct addrinfo hints{}, *res;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    int rc;
    while (running) {
        rc = getaddrinfo(sim_host, std::to_string(sim_port).c_str(), &hints, &res);
        if (rc == 0) break;
        std::cerr << "[controller] Waiting for simulator DNS..." << std::endl;
        sleep(2);
    }
    if (!running) return 0;

    int sock = -1;
    while (running) {
        sock = socket(AF_INET, SOCK_STREAM, 0);
        if (connect(sock, res->ai_addr, res->ai_addrlen) == 0) break;
        close(sock);
        std::cerr << "[controller] Retrying connection..." << std::endl;
        sleep(2);
    }
    freeaddrinfo(res);
    if (!running) return 0;
    std::cout << "[controller] Connected to simulator" << std::endl;

    // Recipe parameters
    double target_temp = 400.0;    // C
    double target_pressure = 2.0;  // Torr
    double target_rf = 300.0;      // W
    double process_time = 60.0;    // seconds
    double gas_flow_recipe[8] = {100, 50, 0, 0, 0, 0, 0, 0}; // SiH4, N2O

    PID temp_pid{2.0, 0.1, 0.5, 0, 0, 0.0, 100.0};
    PID pressure_pid{5.0, 0.2, 1.0, 0, 0, 0.0, 100.0};

    ChamberState state = ChamberState::IDLE;
    SimPacket pkt{};
    uint32_t seq = 0;
    double state_timer = 0;
    double dt = 0.1; // 100ms cycle

    // Start recipe immediately
    state = ChamberState::PUMP_DOWN;
    std::cout << "[controller] Starting recipe -> PUMP_DOWN" << std::endl;

    while (running) {
        // Read sensor data from simulator
        pkt.seq = seq++;
        ssize_t n = recv(sock, &pkt, sizeof(pkt), MSG_WAITALL);
        if (n <= 0) {
            std::cerr << "[controller] Lost connection to simulator" << std::endl;
            break;
        }

        // State machine
        ChamberState prev = state;
        switch (state) {
            case ChamberState::PUMP_DOWN:
                pkt.heater_power = temp_pid.compute(target_temp, pkt.temperature, dt);
                pkt.throttle_pos = 100.0; // fully open
                pkt.rf_setpoint = 0;
                std::memset(pkt.mfc_setpoints, 0, sizeof(pkt.mfc_setpoints));
                if (pkt.pressure < 0.01) {
                    state = ChamberState::GAS_STABILIZE;
                    state_timer = 0;
                }
                break;

            case ChamberState::GAS_STABILIZE:
                pkt.heater_power = temp_pid.compute(target_temp, pkt.temperature, dt);
                pkt.throttle_pos = pressure_pid.compute(target_pressure, pkt.pressure, dt);
                pkt.rf_setpoint = 0;
                std::memcpy(pkt.mfc_setpoints, gas_flow_recipe, sizeof(gas_flow_recipe));
                state_timer += dt;
                if (state_timer > 5.0 && std::abs(pkt.pressure - target_pressure) < 0.5) {
                    state = ChamberState::PROCESS;
                    state_timer = 0;
                }
                break;

            case ChamberState::PROCESS:
                pkt.heater_power = temp_pid.compute(target_temp, pkt.temperature, dt);
                pkt.throttle_pos = pressure_pid.compute(target_pressure, pkt.pressure, dt);
                pkt.rf_setpoint = target_rf;
                std::memcpy(pkt.mfc_setpoints, gas_flow_recipe, sizeof(gas_flow_recipe));
                state_timer += dt;
                if (state_timer >= process_time) {
                    state = ChamberState::PURGE;
                    state_timer = 0;
                }
                // Interlock: over-temperature
                if (pkt.temperature > 500.0) {
                    std::cerr << "[controller] INTERLOCK: Over-temperature!" << std::endl;
                    state = ChamberState::FAULT;
                }
                break;

            case ChamberState::PURGE:
                pkt.heater_power = 0;
                pkt.throttle_pos = 100.0;
                pkt.rf_setpoint = 0;
                std::memset(pkt.mfc_setpoints, 0, sizeof(pkt.mfc_setpoints));
                pkt.mfc_setpoints[1] = 200; // N2 purge
                state_timer += dt;
                if (state_timer > 10.0) {
                    state = ChamberState::VENT;
                    state_timer = 0;
                }
                break;

            case ChamberState::VENT:
                pkt.heater_power = 0;
                pkt.throttle_pos = 0;
                pkt.rf_setpoint = 0;
                std::memset(pkt.mfc_setpoints, 0, sizeof(pkt.mfc_setpoints));
                state_timer += dt;
                if (pkt.pressure > 700.0) {
                    state = ChamberState::IDLE;
                    std::cout << "[controller] Recipe complete" << std::endl;
                    running = false;
                }
                break;

            case ChamberState::FAULT:
                pkt.heater_power = 0;
                pkt.throttle_pos = 100.0;
                pkt.rf_setpoint = 0;
                std::memset(pkt.mfc_setpoints, 0, sizeof(pkt.mfc_setpoints));
                break;

            case ChamberState::IDLE:
                break;
        }

        if (state != prev) {
            std::cout << "[controller] " << state_name(prev) << " -> " << state_name(state) << std::endl;
        }

        if (seq % 10 == 0) {
            std::cout << "[controller] seq=" << seq
                      << " state=" << state_name(state)
                      << " T=" << pkt.temperature
                      << " P=" << pkt.pressure
                      << " RF=" << pkt.rf_forward
                      << std::endl;
        }

        // Send actuator commands back
        n = send(sock, &pkt, sizeof(pkt), 0);
        if (n <= 0) break;
    }

    close(sock);
    std::cout << "[controller] Shutdown" << std::endl;
    return 0;
}
