#pragma once
#include "globals.h"

void startAccessPoint();
bool tryStationConnect();
void connectOrStartAP();
void setupOTA();
void handleOtaPage();
void handleOtaUpload();
