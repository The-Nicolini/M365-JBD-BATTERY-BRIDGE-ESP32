#include "hardware.h"

#if defined(ESP32) || defined(ESP32S3) || defined(ESP32C3)
Preferences prefs;
HardwareSerial jbdSerial(1);
HardwareSerial m365Serial(2);
#elif defined(ESP8266)
SoftwareSerial jbdSerial(JBD_RX, JBD_TX);
SoftwareSerial m365Serial(M365_RX, M365_TX);
#endif

void hardwareInit() {
#if defined(ESP32) || defined(ESP32S3) || defined(ESP32C3)
    prefs.begin("bridge", false);
#elif defined(ESP8266)
    EEPROM.begin(512);
#endif
}


