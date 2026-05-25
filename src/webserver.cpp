#include "protocol.h"
#include "webserver_internal.h"
#include <Arduino.h>
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32) || defined(ESP32S3) || defined(ESP32C3)
#include <WiFi.h>
#include <WebServer.h>
#elif defined(ARDUINO_ARCH_ESP8266) || defined(ESP8266)
#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#endif

char apSsid[33] = "M365-BMS";
char apPass[64] = "m365batt";
bool apRestartPending = false;
bool apActive = true;

// Serve MotorInfo (heartbeat) as JSON
void handleHeartbeat() {
    if (!lastMotorInfo.valid) {
        webServer.send(404, "application/json", "{}\n");
        return;
    }
    String j = "{";
    j += "\"battery_percent\":" + String(lastMotorInfo.battery_percent) + ",";
    j += "\"speed_kmh\":" + String(lastMotorInfo.speed_kmh, 2) + ",";
    j += "\"speed_average_kmh\":" + String(lastMotorInfo.speed_average_kmh, 2) + ",";
    j += "\"total_distance_m\":" + String(lastMotorInfo.total_distance_m) + ",";
    j += "\"trip_distance_m\":" + String(lastMotorInfo.trip_distance_m) + ",";
    j += "\"uptime_s\":" + String(lastMotorInfo.uptime_s) + ",";
    j += "\"frame_temperature\":" + String(lastMotorInfo.frame_temperature, 1);
    j += "}";
    webServer.sendHeader("Cache-Control", "no-cache");
    webServer.send(200, "application/json", j);
}

// Forward declarations for HTTP handlers (to be moved from main.cpp)
void handleRoot();
void handleData();
void handleBtToggle();
void handleKillToggle();
void handleSettings();

#if defined(ESP32) || defined(ESP32S3) || defined(ESP32C3)
WebServer webServer(80);
#elif defined(ESP8266)
ESP8266WebServer webServer(80);
#endif

void webserverInit() {
    WiFi.softAP(apSsid, apPass);
    IPAddress ip = WiFi.softAPIP();
    Serial.printf("[WiFi] AP '%s' ready — open http://%s\n", apSsid, ip.toString().c_str());
    webServer.on("/",         HTTP_GET,  handleRoot);
    webServer.on("/data",     HTTP_GET,  handleData);
    webServer.on("/bt",       HTTP_POST, handleBtToggle);
    webServer.on("/kill",     HTTP_POST, handleKillToggle);
    webServer.on("/settings", HTTP_POST, handleSettings);
    webServer.on("/heartbeat", HTTP_GET, handleHeartbeat);
    webServer.begin();
    Serial.println("[WiFi] HTTP server started on port 80");
}

void webserverLoop() {
    if (apActive) webServer.handleClient();
}

void triggerApRestart() {
    apRestartPending = true;
}
