#pragma once
#include "globals.h"

void decode_basic_info(const uint8_t *frame, size_t len);
void decode_cell_voltages(const uint8_t *frame, size_t len);
void decode_model_string(const uint8_t *frame, size_t len);
void updateM365Data();
void m365Send(NinebotMessage &msg);
void onNinebotMessage(NinebotMessage &msg);
void processM365Serial();
void process_uart_data();
void send_query(const uint8_t *data, size_t len, const char *name);
String buildStatusJson();
