#include "persistence.h"
#include <EEPROM.h>

void saveSettings() {
    EEPROM.put(0, g_Settings);
    if (EEPROM.commit()) {
        Serial.println("[EEPROM] settings saved");
    } else {
        Serial.println("[EEPROM] settings save failed");
    }
}

void loadSettings() {
    BMSSettings loaded;
    EEPROM.get(0, loaded);
    if (loaded.header[0] != g_Settings.header[0] || loaded.header[1] != g_Settings.header[1] || loaded.version != SETTINGS_VERSION) {
        EEPROM.put(0, g_Settings);
        EEPROM.commit();
        Serial.println("[EEPROM] default settings initialized");
    } else {
        memcpy(&g_Settings, &loaded, sizeof(g_Settings));
        if (g_Settings.warning_voltage < 2500 || g_Settings.warning_voltage > 4500) {
            g_Settings.warning_voltage = 3100;
        }
        if (g_Settings.critical_voltage < 2500 || g_Settings.critical_voltage > 4500) {
            g_Settings.critical_voltage = 2900;
        }
        if (g_Settings.warning_charge_current > 10000) {
            g_Settings.warning_charge_current = 2000;
        }
        if (g_Settings.warning_discharge_current > 20000) {
            g_Settings.warning_discharge_current = 4000;
        }
        Serial.println("[EEPROM] settings loaded");
    }
}

void resetSettings() {
    BMSSettings defaults = {};
    WifiSettings wifiDefaults = {};
    memcpy(&g_Settings, &defaults, sizeof(g_Settings));
    memcpy(&wifiSettings, &wifiDefaults, sizeof(wifiSettings));
    EEPROM.put(0, g_Settings);
    EEPROM.put(sizeof(g_Settings), wifiSettings);
    if (EEPROM.commit()) {
        Serial.println("[EEPROM] settings reset to defaults");
    } else {
        Serial.println("[EEPROM] settings reset failed");
    }
}

void saveWifiSettings() {
    const int offset = sizeof(g_Settings);
    EEPROM.put(offset, wifiSettings);
    if (EEPROM.commit()) {
        Serial.println("[EEPROM] wifi settings saved");
    } else {
        Serial.println("[EEPROM] wifi settings save failed");
    }
}

void loadWifiSettings() {
    const int offset = sizeof(g_Settings);
    WifiSettings loaded;
    EEPROM.get(offset, loaded);
    if (loaded.header[0] != wifiSettings.header[0] || loaded.header[1] != wifiSettings.header[1] || loaded.version != SETTINGS_VERSION) {
        WifiSettings defaults = {};
        memcpy(&wifiSettings, &defaults, sizeof(wifiSettings));
        EEPROM.put(offset, wifiSettings);
        EEPROM.commit();
        Serial.println("[EEPROM] default wifi settings initialized");
        return;
    }
    memcpy(&wifiSettings, &loaded, sizeof(wifiSettings));
    if (wifiSettings.ap_timeout_seconds == 0xFFFFu || wifiSettings.ap_timeout_seconds > 86400u) {
        wifiSettings.ap_timeout_seconds = 300;
    }
    if (wifiSettings.station_reconnect_interval == 0 || wifiSettings.station_reconnect_interval > 3600) {
        wifiSettings.station_reconnect_interval = 60;
    }
    if (wifiSettings.max_reconnect_attempts == 0 || wifiSettings.max_reconnect_attempts > 20) {
        wifiSettings.max_reconnect_attempts = 3;
    }
    if (wifiSettings.status_refresh_interval_ms < 200 || wifiSettings.status_refresh_interval_ms > 10000) {
        wifiSettings.status_refresh_interval_ms = 1500;
    }
    if (wifiSettings.scan_timeout_seconds == 0 || wifiSettings.scan_timeout_seconds > 60) {
        wifiSettings.scan_timeout_seconds = 10;
    }
    if (wifiSettings.device_name[0] == '\0') {
        strncpy(wifiSettings.device_name, "M365toJBD", sizeof(wifiSettings.device_name) - 1);
        wifiSettings.device_name[sizeof(wifiSettings.device_name) - 1] = '\0';
    }
    if (wifiSettings.static_ip[0] == '\0') {
        strncpy(wifiSettings.static_ip, "192.168.4.2", sizeof(wifiSettings.static_ip) - 1);
        wifiSettings.static_ip[sizeof(wifiSettings.static_ip) - 1] = '\0';
    }
    if (wifiSettings.gateway[0] == '\0') {
        strncpy(wifiSettings.gateway, "192.168.4.1", sizeof(wifiSettings.gateway) - 1);
        wifiSettings.gateway[sizeof(wifiSettings.gateway) - 1] = '\0';
    }
    if (wifiSettings.subnet[0] == '\0') {
        strncpy(wifiSettings.subnet, "255.255.255.0", sizeof(wifiSettings.subnet) - 1);
        wifiSettings.subnet[sizeof(wifiSettings.subnet) - 1] = '\0';
    }
    if (wifiSettings.dns[0] == '\0') {
        strncpy(wifiSettings.dns, "8.8.8.8", sizeof(wifiSettings.dns) - 1);
        wifiSettings.dns[sizeof(wifiSettings.dns) - 1] = '\0';
    }
    Serial.println("[EEPROM] wifi settings loaded");
}
