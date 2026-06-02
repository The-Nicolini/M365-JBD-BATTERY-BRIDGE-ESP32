#include "wifi_network.h"
#include <WiFi.h>
#include <ArduinoOTA.h>

void startAccessPoint() {
    WiFi.mode(WIFI_AP);
    IPAddress local_ip(192, 168, 4, 1);
    IPAddress gateway(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(local_ip, gateway, subnet);
    WiFi.softAP(current_ssid.c_str(), current_password.c_str());
    Serial.printf("Access point started: %s / %s\n", current_ssid.c_str(), current_password.c_str());
    Serial.printf("AP IP address: %s\n", WiFi.softAPIP().toString().c_str());
}

bool tryStationConnect() {
    Serial.printf("Attempting station connect to '%s'\n", station_ssid.c_str());
    WiFi.mode(WIFI_STA);
    WiFi.disconnect(true);
    WiFi.begin(station_ssid.c_str(), station_password.c_str());

    for (int attempt = 1; attempt <= 3; ++attempt) {
        unsigned long start = millis();
        while (millis() - start < 4000) {
            if (WiFi.status() == WL_CONNECTED) {
                Serial.printf("Connected to existing network '%s' IP=%s\n", station_ssid.c_str(), WiFi.localIP().toString().c_str());
                return true;
            }
            delay(200);
        }
        Serial.printf("Station connect attempt %d failed\n", attempt);
    }
    Serial.println("Existing network not found, falling back to AP mode");
    return false;
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
    ArduinoOTA.setHostname("M365toJBD-OTA");
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
