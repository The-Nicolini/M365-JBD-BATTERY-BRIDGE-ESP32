#include "globals.h"
#include <WiFi.h>
#include "ArduinoOTA.h"
#include "EEPROM.h"
#include <WebServer.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "wifi_network.h"
#include "webui.h"
#include "bms.h"
#include "persistence.h"

HardwareSerial JbdSerial(2);
HardwareSerial M365Serial(1);
uint8_t frame_buf[FRAME_BUF_SIZE];
size_t frame_len = 0;
unsigned long next_query = 0;
unsigned long lastStationReconnect = 0;
uint8_t next_query_index = 0;

WebServer server(80);
String current_ssid = "M365toJBD";
String current_password = "12345678";
String station_ssid = "";
String station_password = "";
bool use_existing_network = false;
bool ota_enabled = false;
uint32_t M365BaudRate = M365_UART_BAUD;

BmsStatus bms;
BMSSettings g_Settings;
M365BMS g_M365BMS;
bool g_M365OvertempTest = false;
WifiSettings wifiSettings;

void setup() {
    Serial.begin(115200);
    delay(100);
    Serial.println("Starting JBD BMS bridge");
    Serial.printf("UART2 RX=%d TX=%d @%d\n", JBD_UART_RX_PIN, JBD_UART_TX_PIN, JBD_UART_BAUD);
    Serial.printf("UART1 RX=%d TX=%d @%d\n", M365_UART_RX_PIN, M365_UART_TX_PIN, M365BaudRate);

    EEPROM.begin(sizeof(g_Settings) + sizeof(wifiSettings));
    loadSettings();
    loadWifiSettings();

    current_ssid = String(wifiSettings.ap_ssid);
    current_password = String(wifiSettings.ap_password);
    station_ssid = String(wifiSettings.station_ssid);
    station_password = String(wifiSettings.station_password);
    use_existing_network = wifiSettings.use_existing;
    ota_enabled = wifiSettings.ota_enabled;
    M365BaudRate = g_Settings.m365_baud;

    // Start UART comms first using persisted settings
    JbdSerial.begin(JBD_UART_BAUD, SERIAL_8N1, JBD_UART_RX_PIN, JBD_UART_TX_PIN);
    M365Serial.begin(M365BaudRate, SERIAL_8N1, M365_UART_RX_PIN, M365_UART_TX_PIN);
    Serial.printf("Sketch running on core %d\n", xPortGetCoreID());

    // Perform BMS conversion / data initialization before bringing up Wi-Fi
    updateM365Data();
    connectOrStartAP();
    registerWebRoutes();
    server.begin();

    if (ota_enabled) {
        setupOTA();
    }

    next_query = millis();
}

void loop() {
    server.handleClient();
    if (ota_enabled) {
        ArduinoOTA.handle();
    }
    checkApTimeout();
    process_uart_data();
    processM365Serial();

    if (use_existing_network && station_ssid.length() > 0) {
        if (stationReconnectActive()) {
            processStationReconnect();
        } else if (WiFi.status() != WL_CONNECTED) {
            unsigned long reconnectIntervalMs = (unsigned long)wifiSettings.station_reconnect_interval * 1000UL;
            if (reconnectIntervalMs < 1000UL) {
                reconnectIntervalMs = 1000UL;
            }
            if (millis() - lastStationReconnect >= reconnectIntervalMs) {
                lastStationReconnect = millis();
                startStationConnect();
            }
        }
    }

    if (millis() >= next_query) {
        unsigned long interval = g_Settings.bms_poll_interval_ms;
        if (interval < 200) {
            interval = JBD_QUERY_INTERVAL_MS;
        }
        next_query += interval;
        switch (next_query_index) {
            case 0:
                send_query(REQUEST_BASIC, sizeof(REQUEST_BASIC), "basic info");
                break;
            case 1:
                send_query(REQUEST_CELLS, sizeof(REQUEST_CELLS), "cell voltages");
                break;
            default:
                send_query(REQUEST_MODEL, sizeof(REQUEST_MODEL), "model string");
                break;
        }
        next_query_index = (next_query_index + 1) % 3;
    }
}
