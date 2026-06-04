#include "wifi_network.h"
#include <WiFi.h>
#include <ArduinoOTA.h>
#include "bms.h"

static unsigned long apStartTime = 0;
static bool apHadClient = false;
static bool stationConnectInProgress = false;
static int stationConnectAttempt = 0;
static unsigned long stationConnectStartMs = 0;

static bool parseIpAddress(const char *value, IPAddress &out) {
    if (value == nullptr || value[0] == '\0') {
        return false;
    }
    return out.fromString(String(value));
}

static bool shouldVerboseLog() {
    return wifiSettings.verbose_logging != 0;
}

void startAccessPoint() {
    WiFi.mode(WIFI_AP);
    if (wifiSettings.device_name[0] != '\0') {
        WiFi.softAPsetHostname(wifiSettings.device_name);
    }
    IPAddress local_ip(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_ip, gateway, subnet);
    WiFi.softAP(current_ssid.c_str(), current_password.c_str());
    apStartTime = millis();
    apHadClient = false;
    if (shouldVerboseLog()) {
        Serial.printf("Access point started: %s / %s\n", current_ssid.c_str(), current_password.c_str());
        Serial.printf("AP IP address: %s\n", WiFi.softAPIP().toString().c_str());
    }
}

void checkApTimeout() {
    if (wifiSettings.ap_timeout_seconds == 0) {
        return;
    }
    if (WiFi.getMode() != WIFI_AP) {
        return;
    }
    if (apStartTime == 0) {
        return;
    }
    int stationCount = WiFi.softAPgetStationNum();
    if (stationCount > 0) {
        apHadClient = true;
        return;
    }
    if (apHadClient) {
        return;
    }
    unsigned long elapsed = millis() - apStartTime;
    unsigned long timeoutMs = (unsigned long)wifiSettings.ap_timeout_seconds * 1000UL;
    if (elapsed >= timeoutMs) {
        Serial.printf("AP disabled after %u seconds with no connected client\n", wifiSettings.ap_timeout_seconds);
        WiFi.softAPdisconnect(true);
        apStartTime = 0;
    }
}

bool tryStationConnect() {
    if (shouldVerboseLog()) {
        Serial.printf("Attempting station connect to '%s'\n", station_ssid.c_str());
    }
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    if (wifiSettings.use_static_ip) {
        IPAddress ip, gw, mask, dns;
        if (parseIpAddress(wifiSettings.static_ip, ip) && parseIpAddress(wifiSettings.gateway, gw) && parseIpAddress(wifiSettings.subnet, mask) && parseIpAddress(wifiSettings.dns, dns)) {
            if (WiFi.config(ip, gw, mask, dns)) {
                if (shouldVerboseLog()) {
                    Serial.printf("Station static IP configured: %s\n", ip.toString().c_str());
                }
            } else if (shouldVerboseLog()) {
                Serial.println("Station static IP configuration failed, using DHCP");
            }
        } else if (shouldVerboseLog()) {
            Serial.println("Invalid static IP settings, using DHCP");
        }
    }
    WiFi.begin(station_ssid.c_str(), station_password.c_str());

    for (int attempt = 1; attempt <= wifiSettings.max_reconnect_attempts; ++attempt) {
        unsigned long start = millis();
        while (millis() - start < 4000) {
            if (WiFi.status() == WL_CONNECTED) {
                if (shouldVerboseLog()) {
                    Serial.printf("Connected to existing network '%s' IP=%s\n", station_ssid.c_str(), WiFi.localIP().toString().c_str());
                }
                return true;
            }
            process_uart_data();
            processM365Serial();
            delay(20);
        }
        if (shouldVerboseLog()) {
            Serial.printf("Station connect attempt %d failed\n", attempt);
        }
    }
    if (shouldVerboseLog()) {
        Serial.println("Existing network not found, falling back to AP mode");
    }
    return false;
}

void startStationConnect() {
    if (shouldVerboseLog()) {
        Serial.printf("Starting station reconnect to '%s'\n", station_ssid.c_str());
    }
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    if (wifiSettings.use_static_ip) {
        IPAddress ip, gw, mask, dns;
        if (parseIpAddress(wifiSettings.static_ip, ip) && parseIpAddress(wifiSettings.gateway, gw) && parseIpAddress(wifiSettings.subnet, mask) && parseIpAddress(wifiSettings.dns, dns)) {
            if (WiFi.config(ip, gw, mask, dns)) {
                if (shouldVerboseLog()) {
                    Serial.printf("Station static IP configured: %s\n", ip.toString().c_str());
                }
            } else if (shouldVerboseLog()) {
                Serial.println("Station static IP configuration failed, using DHCP");
            }
        } else if (shouldVerboseLog()) {
            Serial.println("Invalid static IP settings, using DHCP");
        }
    }
    WiFi.begin(station_ssid.c_str(), station_password.c_str());
    stationConnectInProgress = true;
    stationConnectAttempt = 1;
    stationConnectStartMs = millis();
}

bool stationReconnectActive() {
    return stationConnectInProgress;
}

void processStationReconnect() {
    if (!stationConnectInProgress) {
        return;
    }
    if (WiFi.status() == WL_CONNECTED) {
        if (shouldVerboseLog()) {
            Serial.printf("Station reconnect succeeded: %s IP=%s\n", station_ssid.c_str(), WiFi.localIP().toString().c_str());
        }
        stationConnectInProgress = false;
        return;
    }
    if (millis() - stationConnectStartMs < 4000) {
        process_uart_data();
        processM365Serial();
        return;
    }
    if (stationConnectAttempt >= wifiSettings.max_reconnect_attempts) {
        if (shouldVerboseLog()) {
            Serial.println("Station reconnect failed, switching to AP mode");
        }
        stationConnectInProgress = false;
        startAccessPoint();
        return;
    }
    stationConnectAttempt++;
    stationConnectStartMs = millis();
    if (shouldVerboseLog()) {
        Serial.printf("Station reconnect attempt %d\n", stationConnectAttempt);
    }
    WiFi.begin(station_ssid.c_str(), station_password.c_str());
}

void connectOrStartAP() {
    if (use_existing_network && station_ssid.length() > 0) {
        if (tryStationConnect()) {
            return;
        }
    }
    startAccessPoint();
}

void setupOTA() {
    if (wifiSettings.device_name[0] != '\0') {
        ArduinoOTA.setHostname(wifiSettings.device_name);
    }
    if (wifiSettings.ota_password[0] != '\0') {
        ArduinoOTA.setPassword(wifiSettings.ota_password);
    }
    ArduinoOTA.onStart([]() {
        Serial.println("OTA start");
    });
    ArduinoOTA.onEnd([]() {
        Serial.println("OTA end");
    });
    ArduinoOTA.onError([](ota_error_t error) {
        Serial.printf("OTA error[%u]\n", error);
    });
    ArduinoOTA.begin();
}

void handleOtaPage() {
    if (!ota_enabled) {
        server.send(403, "text/plain", "OTA updates are not enabled");
        return;
    }
    if (wifiSettings.ota_access_only_on_ap && WiFi.getMode() != WIFI_AP) {
        server.send(403, "text/plain", "OTA access is only allowed while the device is in AP mode");
        return;
    }
    const char *html = R"rawliteral(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width,initial-scale=1.0">
<title>OTA Upload</title>
<style>
body{margin:0;font-family:Inter,system-ui,sans-serif;background:#07101F;color:#E8F1FF;}
header{padding:20px 24px 16px;background:#0E1B32;border-bottom:1px solid rgba(255,255,255,.08);}
h1{margin:0;font-size:1.8rem;}
main{padding:24px;}
form{display:grid;gap:18px;max-width:520px;}
label{display:grid;gap:8px;font-size:.95rem;color:#A7B8D6;}
input[type=file]{color:#E8F1FF;}
button{border:none;padding:12px 16px;border-radius:14px;background:#3C6DE0;color:#fff;font-weight:700;cursor:pointer;}
button:hover{background:#5A82F5;}
a{color:#7CA8FF;text-decoration:none;}
</style>
</head>
<body>
<header><h1>OTA Upload</h1></header>
<main>
<form method="POST" action="/ota" enctype="multipart/form-data">
<label>Firmware file<input type="file" name="update"></label>
<button type="submit">Upload and install</button>
</form>
<p><a href="/settings">Back to settings</a></p>
</main>
</body>
</html>)rawliteral";
    server.send(200, "text/html", html);
}

void handleOtaUpload() {
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        Serial.printf("OTA upload start: %s\n", upload.filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
            Update.printError(Serial);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) {
            Serial.printf("OTA upload complete: %u bytes\n", upload.totalSize);
        } else {
            Update.printError(Serial);
        }
    }
}
