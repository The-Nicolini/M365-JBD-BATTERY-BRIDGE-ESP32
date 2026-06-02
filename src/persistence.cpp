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
    Serial.println("[EEPROM] wifi settings loaded");
}
