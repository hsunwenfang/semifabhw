"""CVD Chamber Physics Simulator - TCP server that models thermal, pressure, and plasma behavior."""

import struct
import socket
import signal
import sys
import os
import math
import time

# SimPacket: seq(I) + timestamp(d) + 12 doubles sensors + 11 doubles actuators = 4 + 23*8 = 188 bytes
# Match C++ struct layout exactly
PACKET_FMT = "<Id" + "d" * 12 + "d" * 11  # little-endian
PACKET_SIZE = struct.calcsize(PACKET_FMT)

running = True
def handle_signal(sig, frame):
    global running
    running = False

signal.signal(signal.SIGINT, handle_signal)
signal.signal(signal.SIGTERM, handle_signal)


class ChamberModel:
    """Simplified CVD chamber physics model."""

    def __init__(self):
        # State
        self.temperature = 25.0       # C (room temp)
        self.pressure = 760.0         # Torr (atmospheric)
        self.rf_forward = 0.0         # W
        self.rf_reflected = 0.0       # W
        self.gas_flows = [0.0] * 8    # sccm actual

        # Physical parameters
        self.thermal_mass = 50.0      # J/C (susceptor + chamber)
        self.thermal_loss = 0.5       # W/C (radiation + conduction)
        self.heater_max_power = 5000  # W
        self.chamber_volume = 10.0    # liters
        self.pump_speed = 500.0       # l/s (turbo pump)
        self.mfc_max_flow = 500.0     # sccm per channel

    def step(self, dt: float, heater_power: float, throttle_pos: float,
             rf_setpoint: float, mfc_setpoints: list) -> None:
        """Advance physics by dt seconds."""

        # --- Thermal model ---
        heat_in = (heater_power / 100.0) * self.heater_max_power
        # RF adds ~30% of forward power as heat
        heat_in += self.rf_forward * 0.3
        heat_loss = self.thermal_loss * (self.temperature - 25.0)
        self.temperature += (heat_in - heat_loss) * dt / self.thermal_mass
        self.temperature = max(self.temperature, 25.0)

        # --- Gas flow model ---
        total_gas_in = 0.0  # sccm
        for i in range(8):
            sp = max(0.0, min(mfc_setpoints[i], self.mfc_max_flow))
            # MFC responds with ~0.5s time constant
            self.gas_flows[i] += (sp - self.gas_flows[i]) * min(1.0, dt / 0.5)
            total_gas_in += self.gas_flows[i]

        # --- Pressure model ---
        # Convert sccm to Torr*L/s: 1 sccm = 1.27e-3 Torr*L/s at room temp
        gas_throughput = total_gas_in * 1.27e-3
        # Effective pump speed depends on throttle position
        eff_pump_speed = self.pump_speed * (throttle_pos / 100.0)
        pump_throughput = eff_pump_speed * self.pressure / 760.0  # simplified

        dp = (gas_throughput - pump_throughput) / self.chamber_volume
        self.pressure += dp * dt
        self.pressure = max(self.pressure, 1e-6)  # hard vacuum limit

        # If no pumping and no gas, slow vent to atmosphere
        if throttle_pos < 1.0 and total_gas_in < 0.1:
            self.pressure += (760.0 - self.pressure) * 0.01 * dt

        # --- RF/Plasma model ---
        if rf_setpoint > 0 and self.pressure > 0.1 and self.pressure < 50.0:
            # Plasma can strike in this pressure range
            self.rf_forward = rf_setpoint
            self.rf_reflected = rf_setpoint * 0.05  # 5% reflected (good match)
        elif rf_setpoint > 0:
            # Can't strike plasma outside pressure window
            self.rf_forward = 0
            self.rf_reflected = rf_setpoint  # all reflected
        else:
            self.rf_forward = 0
            self.rf_reflected = 0

    def get_sensors(self):
        return (self.temperature, self.pressure, self.rf_forward, self.rf_reflected,
                *self.gas_flows)


def main():
    port = int(os.environ.get("SIM_PORT", "5555"))
    dt = 0.1  # 100ms simulation step

    server = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    server.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    server.bind(("0.0.0.0", port))
    server.listen(1)
    server.settimeout(2.0)
    print(f"[simulator] Listening on port {port}")

    while running:
        try:
            conn, addr = server.accept()
        except socket.timeout:
            continue
        print(f"[simulator] Controller connected from {addr}")

        model = ChamberModel()
        seq = 0
        sim_time = 0.0

        try:
            while running:
                # Send sensor data
                sensors = model.get_sensors()
                pkt = struct.pack(PACKET_FMT,
                    seq, sim_time,
                    *sensors,
                    0.0, 0.0, 0.0,  # placeholder actuator fields
                    *([0.0] * 8))
                conn.sendall(pkt)

                # Receive actuator commands
                data = b""
                while len(data) < PACKET_SIZE:
                    chunk = conn.recv(PACKET_SIZE - len(data))
                    if not chunk:
                        raise ConnectionError("Controller disconnected")
                    data += chunk

                vals = struct.unpack(PACKET_FMT, data)
                # Extract actuator commands (after seq + timestamp + 12 sensor doubles)
                idx = 14  # 2 header + 12 sensors
                heater_power = vals[idx]
                throttle_pos = vals[idx + 1]
                rf_setpoint = vals[idx + 2]
                mfc_setpoints = list(vals[idx + 3: idx + 11])

                model.step(dt, heater_power, throttle_pos, rf_setpoint, mfc_setpoints)

                sim_time += dt
                seq += 1

                if seq % 10 == 0:
                    print(f"[simulator] t={sim_time:.1f}s T={model.temperature:.1f}C "
                          f"P={model.pressure:.3f}Torr RF={model.rf_forward:.0f}W")

                time.sleep(dt)

        except (ConnectionError, BrokenPipeError) as e:
            print(f"[simulator] Connection lost: {e}")
        finally:
            conn.close()

    server.close()
    print("[simulator] Shutdown")


if __name__ == "__main__":
    main()
