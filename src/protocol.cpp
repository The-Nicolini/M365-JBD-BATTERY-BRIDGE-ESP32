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

uint8_t build_write_cruise(uint8_t* out, bool enable) {
    uint8_t param[2] = {enable ? 0x01 : 0x00, 0x00};
    return build_m365_packet(out, M365_ADDR_MASTER, M365_ADDR_SCOOTER, M365_CMD_CRUISE, param, 2);
}

uint8_t build_write_led(uint8_t* out, bool enable) {
    uint8_t param[2] = {enable ? 0x01 : 0x00, 0x00};
    return build_m365_packet(out, M365_ADDR_MASTER, M365_ADDR_SCOOTER, M365_CMD_LED, param, 2);
}

uint16_t m365_crc(const uint8_t* data, uint8_t len) {
    uint16_t sum = 0;
    for (uint8_t i = 0; i < len; i++) sum += data[i];
    return (~sum) & 0xFFFF;
}

uint8_t build_m365_packet(uint8_t* out, uint8_t src, uint8_t dst, uint8_t cmd, const uint8_t* data, uint8_t dataLen) {
    out[0] = M365_HEADER1;
    out[1] = M365_HEADER2;
    out[2] = 3 + dataLen;
    out[3] = src;
    out[4] = dst;
    out[5] = cmd;
    if (dataLen > 0 && data) {
        memcpy(&out[6], data, dataLen);
    }
    uint16_t crc = m365_crc(&out[2], 4 + dataLen);
    out[6 + dataLen] = crc & 0xFF;
    out[7 + dataLen] = (crc >> 8) & 0xFF;
    return 8 + dataLen;
}

// Move pollJbd, processM365Input, buildBmsStatusPacket, buildBmsCellPacket, and helpers from main.cpp here
