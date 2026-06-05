#pragma once
#include <Arduino.h>
#include <WebServer.h>

#define M365_UART_RX_PIN 17
#define M365_UART_TX_PIN 18
#define M365_UART_BAUD 115200
#define SETTINGS_VERSION 2

#define JBD_UART_RX_PIN 47
#define JBD_UART_TX_PIN 48
#define JBD_UART_BAUD 9600
#define JBD_QUERY_INTERVAL_MS 1500
#define FRAME_BUF_SIZE 256

static const char *AP_SSID = "M365toJBD";
static const char *AP_PASSWORD = "12345678";

static const uint8_t REQUEST_BASIC[] = {0xDD, 0xA5, 0x03, 0x00, 0xFF, 0xFD, 0x77};
static const uint8_t REQUEST_CELLS[] = {0xDD, 0xA5, 0x04, 0x00, 0xFF, 0xFC, 0x77};
static const uint8_t REQUEST_MODEL[] = {0xDD, 0xA5, 0x05, 0x00, 0xFF, 0xFB, 0x77};

extern HardwareSerial JbdSerial;
extern HardwareSerial M365Serial;
extern uint8_t frame_buf[FRAME_BUF_SIZE];
extern size_t frame_len;
extern unsigned long next_query;
extern uint8_t next_query_index;

extern WebServer server;
extern String current_ssid;
extern String current_password;
extern String station_ssid;
extern String station_password;
extern bool use_existing_network;
extern bool ota_enabled;
extern uint32_t M365BaudRate;

struct BmsStatus {
    bool valid = false;
    float total_voltage = 0.0f;
    float current_a = 0.0f;
    float residual_ah = 0.0f;
    float nominal_ah = 0.0f;
    uint8_t rsoc = 0;
    uint8_t fet = 0;
    uint8_t cell_count = 0;
    uint8_t ntc_count = 0;
    uint16_t cycles = 0;
    uint16_t production_year = 0;
    uint8_t production_month = 0;
    uint8_t production_day = 0;
    uint16_t balance_low = 0;
    uint16_t balance_high = 0;
    uint16_t protection = 0;
    String version;
    String model;
    String protection_text;
    String balance_text;
    int cell_voltage_count = 0;
    float cell_voltages[64];
    int ntc_count_received = 0;
    float ntc_temps[16];
    unsigned long last_update_ms = 0;
};

extern BmsStatus bms;

#define M365BMS_RADDR 0x22
#define M365BMS_WADDR 0x25

struct BMSSettings {
    uint8_t header[2] = {0xB0, 0x0B};
    uint16_t version = SETTINGS_VERSION;
    char serial[14] = "BRIDGE001";
    uint32_t capacity = 7800;
    uint16_t nominal_voltage = 3600;
    uint16_t full_voltage = 4150;
    uint16_t num_cycles = 0;
    uint16_t num_charged = 0;
    uint16_t date = (18 << 9) | (10 << 5) | 1;
    uint16_t shuntResistor_uOhm = 1000;
    uint16_t thermistor_BetaK = 3435;
    int16_t temp_minDischargeC = -20;
    int16_t temp_maxDischargeC = 60;
    int16_t temp_minChargeC = 0;
    int16_t temp_maxChargeC = 45;
    uint32_t SCD_current = 80000;
    uint16_t SCD_delay = 200;
    uint32_t OCD_current = 6000;
    uint16_t OCD_delay = 3000;
    uint32_t ODP_current = 35000;
    uint16_t ODP_delay = 1280;
    uint16_t UVP_voltage = 2800;
    uint16_t UVP_delay = 2;
    uint16_t OVP_voltage = 4200;
    uint16_t OVP_delay = 2;
    uint16_t balance_minIdleTime = 1800;
    uint16_t balance_minVoltage = 3600;
    uint16_t balance_maxVoltageDiff = 10;
    uint16_t idle_currentThres = 500;
    uint16_t balance_enabled = 1;
    int16_t adcPackOffset = 0;
    int16_t adcCellsOffset[15] = {0};
    uint32_t m365_baud = M365_UART_BAUD;
    uint8_t map_to_10_cells = 0;
    uint16_t bms_poll_interval_ms = JBD_QUERY_INTERVAL_MS;
    uint8_t bms_command_retry_count = 3;
    uint16_t warning_voltage = 3100;
    uint16_t critical_voltage = 2900;
    uint16_t warning_charge_current = 2000;
    uint16_t warning_discharge_current = 4000;
} __attribute__((packed));

extern BMSSettings g_Settings;

struct M365BMS {
    uint16_t unk1[16] = {0x5A, 0x5A, 0x00};
    char serial[14] = "BRIDGE001";
    uint16_t version = 0x900;
    uint16_t design_capacity = 7800;
    uint16_t real_capacity = 7800;
    uint16_t nominal_voltage = 3600;
    uint16_t num_cycles = 0;
    uint16_t num_charged = 0;
    uint16_t max_voltage = 0;
    uint16_t max_discharge_current = 0;
    uint16_t max_charge_current = 0;
    uint16_t date = 0;
    uint8_t errors[6] = {0};
    uint16_t unk3[12] = {0};
    uint16_t status = 1;
    uint16_t capacity_left = 0;
    uint16_t percent_left = 0;
    int16_t current = 0;
    uint16_t voltage = 0;
    uint8_t temperature[2] = {0, 0};
    uint16_t balance_bits = 0;
    uint16_t unk5[4] = {0};
    uint16_t health = 100;
    uint16_t unk6[4] = {0};
    uint16_t cell_voltages[15] = {0};
    uint16_t unk7[2] = {0};
} __attribute__((packed));

extern M365BMS g_M365BMS;
extern bool g_M365OvertempTest;

struct WifiSettings {
    uint8_t header[2] = {0xB0, 0x0B};
    uint16_t version = SETTINGS_VERSION;
    char ap_ssid[32] = "M365toJBD";
    char ap_password[32] = "12345678";
    char station_ssid[32] = "";
    char station_password[32] = "";
    uint8_t use_existing = 0;
    uint8_t ota_enabled = 0;
    uint16_t ap_timeout_seconds = 300;
    char device_name[32] = "M365toJBD";
    uint8_t verbose_logging = 0;
    uint8_t use_static_ip = 0;
    char static_ip[16] = "192.168.4.2";
    char gateway[16] = "192.168.4.1";
    char subnet[16] = "255.255.255.0";
    char dns[16] = "8.8.8.8";
    uint16_t station_reconnect_interval = 60;
    uint8_t max_reconnect_attempts = 3;
    char ota_password[32] = "";
    uint8_t ota_access_only_on_ap = 0;
    uint16_t status_refresh_interval_ms = 1500;
    uint16_t scan_timeout_seconds = 10;
} __attribute__((packed));

extern WifiSettings wifiSettings;

struct NinebotMessage {
    uint8_t header[2];
    uint8_t length;
    uint8_t addr;
    uint8_t mode;
    uint8_t offset;
    uint8_t data[253];
    uint16_t checksum;
};

uint16_t be16(const uint8_t *p);
int16_t be16s(const uint8_t *p);
