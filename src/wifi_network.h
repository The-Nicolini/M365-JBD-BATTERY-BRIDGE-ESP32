#pragma once
#include "globals.h"

void startAccessPoint();
bool tryStationConnect();
void startStationConnect();
void processStationReconnect();
bool stationReconnectActive();
void connectOrStartAP();
void checkApTimeout();
void setupOTA();
void handleOtaPage();
void handleOtaUpload();
