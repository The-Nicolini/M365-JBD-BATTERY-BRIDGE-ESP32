#pragma once

#include <Arduino.h>
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32) || defined(ESP32S3) || defined(ESP32C3)
#include <WebServer.h>
#elif defined(ARDUINO_ARCH_ESP8266) || defined(ESP8266)
#include <ESP8266WebServer.h>
#endif

void webserverInit();
void webserverLoop();
void triggerApRestart();
extern char apSsid[33];
extern char apPass[64];
extern bool apRestartPending;
extern bool apActive;
#if defined(ARDUINO_ARCH_ESP32) || defined(ESP32) || defined(ESP32S3) || defined(ESP32C3)
extern WebServer webServer;
#elif defined(ARDUINO_ARCH_ESP8266) || defined(ESP8266)
extern ESP8266WebServer webServer;
#endif
