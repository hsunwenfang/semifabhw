#pragma once
#include <cstdint>

#pragma pack(push, 1)
struct SimPacket {
    uint32_t seq;
    double   timestamp;
    // Sensor readings (simulator -> controller)
    double   temperature;      // C
    double   pressure;         // Torr
    double   rf_forward;       // W
    double   rf_reflected;     // W
    double   gas_flows[8];     // sccm
    // Actuator commands (controller -> simulator)
    double   heater_power;     // 0-100%
    double   throttle_pos;     // 0-100%
    double   rf_setpoint;      // W
    double   mfc_setpoints[8]; // sccm
};
#pragma pack(pop)
