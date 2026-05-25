#pragma once

// Board-specific pin definitions
#if defined(ESP32) || defined(ESP32S3) || defined(ESP32C3)
#define JBD_RX  16
#define JBD_TX  17
#define M365_RX 18
#define M365_TX 19
#elif defined(ESP8266)
// D1 Mini pin mapping: avoid boot-strapping pins like D8/GPIO15 for UART TX.
#define JBD_RX  D5
#define JBD_TX  D6
#define M365_RX D7
#define M365_TX D1
#endif
