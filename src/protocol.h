#define M365_HEADER1 0x55
#define M365_HEADER2 0xAA
#define M365_ADDR_MASTER 0x20
#define M365_ADDR_SCOOTER 0x23
#define M365_ADDR_BATTERY 0x22
#define M365_ADDR_BATTERY_RESP 0x25
#define M365_CMD_SERIAL      0x10
#define M365_CMD_FIRMWARE    0x1A
#define M365_CMD_BMS_VER     0x67
#define M365_CMD_PIN         0x17
#define M365_CMD_TRIP        0x3A
#define M365_CMD_KM_LEFT     0x25
#define M365_CMD_BATT_SPEED  0xB0
#define M365_CMD_CRUISE      0x7C
#define M365_CMD_LED         0x7D
#define M365_CMD_REGEN       0x7B
#define M365_CMD_TEMP        0x3E
#define M365_CMD_SPEED_LIMIT 0x73
#define M365_CMD_BATT_SN     0x10
#define M365_CMD_BATT_CYCLES 0x1B
#define M365_CMD_BATT_MAH    0x31
#define M365_CMD_CELL_VOLT   0x40

#include <cstdint>
#include <cstddef>
// Heartbeat (MotorInfo) struct and state
struct MotorInfo {
    uint16_t battery_percent;
    float speed_kmh;
    float speed_average_kmh;
    uint32_t total_distance_m;
    int16_t trip_distance_m;
    uint16_t uptime_s;
    float frame_temperature;
    bool valid;
};

extern MotorInfo lastMotorInfo;
bool check_and_update_heartbeat();

#pragma once

// Protocol helpers
struct Payload {
    const uint8_t* data;
    size_t len;
    size_t pos;
    Payload(const uint8_t* d, size_t l);
    bool pop_head();
    uint16_t pop_u16();
    int16_t pop_i16();
    uint8_t pad_byte();
    bool pad_bytes(size_t n);
    uint32_t pop_u32();
};

struct BatteryInfo {
    uint16_t capacity;
    uint16_t percent;
    float current;
    float voltage;
    uint8_t temperature_1;
    uint8_t temperature_2;
};

bool parse_motor_info(const uint8_t* payload, size_t len, MotorInfo& out);
bool parse_battery_info(const uint8_t* payload, size_t len, BatteryInfo& out);

enum class M365Direction : uint8_t;
enum class M365ReadWrite : uint8_t;
enum class M365Attribute : uint8_t;

struct M365Session {
    static uint8_t build_command(uint8_t* out, M365Direction dir, M365ReadWrite rw, M365Attribute attr, const uint8_t* data, uint8_t dataLen);
};

uint16_t m365_crc(const uint8_t* data, uint8_t len);
uint8_t build_m365_packet(uint8_t* out, uint8_t src, uint8_t dst, uint8_t cmd, const uint8_t* data, uint8_t dataLen);
uint8_t build_read_serial(uint8_t* out);
uint8_t build_read_firmware(uint8_t* out);
uint8_t build_read_bms_ver(uint8_t* out);
uint8_t build_read_pin(uint8_t* out);
uint8_t build_write_pin(uint8_t* out, const char* pin);
uint8_t build_read_trip(uint8_t* out);
uint8_t build_read_km_left(uint8_t* out);
uint8_t build_read_batt_speed(uint8_t* out);
uint8_t build_read_cruise(uint8_t* out);
uint8_t build_write_cruise(uint8_t* out, bool enable);
uint8_t build_read_led(uint8_t* out);
uint8_t build_write_led(uint8_t* out, bool enable);
uint8_t build_read_regen(uint8_t* out);
uint8_t build_write_regen(uint8_t* out, uint8_t level);
uint8_t build_read_temp(uint8_t* out);
uint8_t build_read_speed_limit(uint8_t* out);
uint8_t build_read_batt_sn(uint8_t* out);
uint8_t build_read_batt_cycles(uint8_t* out);
uint8_t build_read_batt_mah(uint8_t* out);
uint8_t build_read_cell_volt(uint8_t* out);

#include <Arduino.h>

struct BatteryState {
    uint16_t voltage_mv;
    int16_t  current_ma;
    uint16_t remaining_mah;
    uint16_t nominal_mah;
    uint8_t  soc_pct;
    uint8_t  cell_count;
    uint16_t cell_mv[20];
    int16_t  temp_c10[3];
    uint8_t  ntc_count;
    uint16_t prot_status;
    bool     valid;
};

extern BatteryState batt;

void protocolInit();
void pollJbd();
void processM365Input();
uint8_t buildBmsStatusPacket(uint8_t* out);
uint8_t buildBmsCellPacket(uint8_t* out);
