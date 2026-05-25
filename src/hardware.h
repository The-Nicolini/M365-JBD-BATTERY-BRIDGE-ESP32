#pragma once

#include <Arduino.h>
#include "pins.h"

#if defined(ESP32) || defined(ESP32S3) || defined(ESP32C3)
#include <Preferences.h>
extern Preferences prefs;
extern HardwareSerial jbdSerial;
extern HardwareSerial m365Serial;
#elif defined(ESP8266)
#include <EEPROM.h>
#include <SoftwareSerial.h>
extern SoftwareSerial jbdSerial;
extern SoftwareSerial m365Serial;
#endif

void hardwareInit();
