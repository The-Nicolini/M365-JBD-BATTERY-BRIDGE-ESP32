#include "protocol.h"

MotorInfo lastMotorInfo = {};

// Read regen status
uint8_t build_read_regen(uint8_t* out) {
    uint8_t param[2] = {0x02, 0x00};
    return build_m365_packet(out, M365_ADDR_MASTER, M365_ADDR_SCOOTER, M365_CMD_REGEN, param, 2);
}

// Write regen (0=weak, 1=medium, 2=strong)
uint8_t build_write_regen(uint8_t* out, uint8_t level) {
    uint8_t param[2] = {level, 0x00};
    return build_m365_packet(out, M365_ADDR_MASTER, M365_ADDR_SCOOTER, M365_CMD_REGEN, param, 2);
}

// Read temperature
uint8_t build_read_temp(uint8_t* out) {
    uint8_t param[2] = {0x02, 0x00};
    return build_m365_packet(out, M365_ADDR_MASTER, M365_ADDR_SCOOTER, M365_CMD_TEMP, param, 2);
}

// Read speed limit
uint8_t build_read_speed_limit(uint8_t* out) {
    uint8_t param[2] = {0x04, 0x00};
    return build_m365_packet(out, M365_ADDR_MASTER, M365_ADDR_SCOOTER, M365_CMD_SPEED_LIMIT, param, 2);
}

// Read battery serial
uint8_t build_read_batt_sn(uint8_t* out) {
    uint8_t param[2] = {0x12, 0x00};
    return build_m365_packet(out, M365_ADDR_MASTER, M365_ADDR_BATTERY, M365_CMD_BATT_SN, param, 2);
}

// Read battery cycles
uint8_t build_read_batt_cycles(uint8_t* out) {
    uint8_t param[2] = {0x04, 0x00};
    return build_m365_packet(out, M365_ADDR_MASTER, M365_ADDR_BATTERY, M365_CMD_BATT_CYCLES, param, 2);
}

// Read battery mAh, percent, current, voltage, temp
uint8_t build_read_batt_mah(uint8_t* out) {
    uint8_t param[2] = {0x0A, 0x00};
    return build_m365_packet(out, M365_ADDR_MASTER, M365_ADDR_BATTERY, M365_CMD_BATT_MAH, param, 2);
}

// Read cell voltages
uint8_t build_read_cell_volt(uint8_t* out) {
    uint8_t param[2] = {0x1E, 0x00};
    return build_m365_packet(out, M365_ADDR_MASTER, M365_ADDR_BATTERY, M365_CMD_CELL_VOLT, param, 2);
}

// Move pollJbd, processM365Input, buildBmsStatusPacket, buildBmsCellPacket, and helpers from main.cpp here
