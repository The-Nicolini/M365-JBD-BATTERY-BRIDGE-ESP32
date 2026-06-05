#include "bms.h"
#include "persistence.h"

uint16_t be16(const uint8_t *p) {
    return (uint16_t(p[0]) << 8) | uint16_t(p[1]);
}

int16_t be16s(const uint8_t *p) {
    return int16_t(be16(p));
}

bool verify_checksum(const uint8_t *frame, size_t len) {
    if (len < 7 || frame[0] != 0xDD || frame[len - 1] != 0x77) {
        return false;
    }
    size_t checksum_index = len - 3;
    uint32_t sum = 0;
    for (size_t i = 2; i < checksum_index; ++i) {
        sum += frame[i];
    }
    uint16_t expected = uint16_t((0x10000u - (sum & 0xFFFFu)) & 0xFFFFu);
    uint16_t got = uint16_t(frame[checksum_index] << 8) | uint16_t(frame[checksum_index + 1]);
    return expected == got;
}

String format_balance_text(uint16_t low, uint16_t high) {
    String result;
    bool any = false;
    for (int i = 0; i < 16; ++i) {
        if (low & (1u << i)) {
            if (any) result += ", ";
            result += String(i + 1);
            any = true;
        }
    }
    for (int i = 0; i < 16; ++i) {
        if (high & (1u << i)) {
            if (any) result += ", ";
            result += String(i + 17);
            any = true;
        }
    }
    if (!any) {
        return "none";
    }
    return result;
}

String format_protection_text(uint16_t protection) {
    static const char *protection_names[] = {
        "cell overvoltage",
        "cell undervoltage",
        "battery overvoltage",
        "battery undervoltage",
        "charging overtemp",
        "charging undertemp",
        "discharging overtemp",
        "discharging undertemp",
        "charging overcurrent",
        "discharging overcurrent",
        "short circuit",
        "IC error",
        "MOS software lock",
    };
    String result;
    bool any = false;
    for (int i = 0; i < 13; ++i) {
        if (protection & (1u << i)) {
            if (any) result += ", ";
            result += protection_names[i];
            any = true;
        }
    }
    return any ? result : "none";
}

void print_raw_frame(const uint8_t *frame, size_t len) {
    Serial.printf("<- JBD raw frame len=%u: ", len);
    for (size_t i = 0; i < len; ++i) {
        Serial.printf("%02X", frame[i]);
        if (i + 1 < len) {
            Serial.print(':');
        }
    }
    Serial.println();
}

void print_protection_status(uint16_t protection) {
    Serial.print("  protections: ");
    Serial.println(format_protection_text(protection));
}

void print_balance_status(uint16_t low, uint16_t high) {
    Serial.print("  balance on: ");
    Serial.println(format_balance_text(low, high));
}

void decode_basic_info(const uint8_t *frame, size_t len) {
    if (len < 24) {
        Serial.println("Basic info too short");
        return;
    }
    if (!verify_checksum(frame, len)) {
        Serial.println("Basic info checksum failed");
        print_raw_frame(frame, len);
        return;
    }
    const uint8_t *payload = frame + 4;
    size_t payload_len = len - 4 - 3;
    if (payload_len < 23) {
        Serial.println("Basic info payload too short");
        return;
    }
    uint16_t total_mV10 = be16(payload);
    int16_t current_10mA = be16s(payload + 2);
    uint16_t residual_10mAh = be16(payload + 4);
    uint16_t nominal_10mAh = be16(payload + 6);
    uint16_t cycles = be16(payload + 8);
    uint16_t date_raw = be16(payload + 10);
    uint16_t balance = be16(payload + 12);
    uint16_t balance_high = be16(payload + 14);
    uint16_t protection = be16(payload + 16);
    uint8_t version = payload[18];
    uint8_t rsoc = payload[19];
    uint8_t fet = payload[20];
    uint8_t cell_count = payload[21];
    uint8_t ntc_count = payload[22];

    float total_v = total_mV10 / 100.0f;
    float current_a = current_10mA / 100.0f;
    float residual_ah = residual_10mAh / 100.0f;
    float nominal_ah = nominal_10mAh / 100.0f;
    uint8_t day = date_raw & 0x1F;
    uint8_t month = (date_raw >> 5) & 0x0F;
    uint16_t year = 2000 + ((date_raw >> 9) & 0x7F);

    bms.valid = true;
    bms.total_voltage = total_v;
    bms.current_a = current_a;
    bms.residual_ah = residual_ah;
    bms.nominal_ah = nominal_ah;
    bms.cycles = cycles;
    bms.production_year = year;
    bms.production_month = month;
    bms.production_day = day;
    bms.balance_low = balance;
    bms.balance_high = balance_high;
    bms.protection = protection;
    bms.rsoc = rsoc;
    bms.fet = fet;
    bms.cell_count = cell_count;
    bms.ntc_count = ntc_count;
    bms.protection_text = format_protection_text(protection);
    bms.balance_text = format_balance_text(balance, balance_high);
    bms.last_update_ms = millis();

    if (payload_len >= 23 + size_t(ntc_count) * 2) {
        bms.ntc_count_received = ntc_count;
        for (uint8_t i = 0; i < ntc_count && i < 16; ++i) {
            uint16_t raw = be16(payload + 23 + i * 2);
            bms.ntc_temps[i] = (raw - 2731) / 10.0f;
        }
    }

    Serial.printf("JBD basic info: total=%.2fV current=%.2fA soc=%u%% cycles=%u version=0x%02X cells=%u ntc=%u\n",
                  total_v, current_a, rsoc, cycles, version, cell_count, ntc_count);
    Serial.printf("  residual=%.2fAh nominal=%.2fAh\n", residual_ah, nominal_ah);
    print_protection_status(protection);
    print_balance_status(balance, balance_high);
    Serial.printf("  production=%04u-%02u-%02u\n", year, month, day);
    for (uint8_t i = 0; i < bms.ntc_count_received; ++i) {
        Serial.printf("  NTC%u=%.1f°C\n", i + 1, bms.ntc_temps[i]);
    }
    updateM365Data();
}

void decode_cell_voltages(const uint8_t *frame, size_t len) {
    if (len < 8) {
        Serial.println("Cell voltage frame too short");
        return;
    }
    if (!verify_checksum(frame, len)) {
        Serial.println("Cell voltage checksum failed");
        print_raw_frame(frame, len);
        return;
    }
    size_t payload_len = len - 4 - 3;
    if (payload_len % 2 != 0) {
        Serial.println("Cell voltage payload length invalid");
        return;
    }
    uint8_t cell_count = payload_len / 2;
    bms.cell_voltage_count = cell_count;
    bms.last_update_ms = millis();
    Serial.printf("JBD cell voltages: %u cells\n", cell_count);
    for (uint8_t i = 0; i < cell_count && i < 64; ++i) {
        uint16_t mv = be16(frame + 4 + i * 2);
        bms.cell_voltages[i] = mv / 1000.0f;
        Serial.printf("  cell %u = %.3f V\n", i + 1, bms.cell_voltages[i]);
    }
    updateM365Data();
}

void decode_model_string(const uint8_t *frame, size_t len) {
    if (len < 8) {
        Serial.println("Model frame too short");
        return;
    }
    if (!verify_checksum(frame, len)) {
        Serial.println("Model string checksum failed");
        print_raw_frame(frame, len);
        return;
    }
    size_t payload_len = len - 4 - 3;
    bms.model.clear();
    for (size_t i = 0; i < payload_len; ++i) {
        uint8_t c = frame[4 + i];
        if (c >= 0x20 && c <= 0x7E) {
            bms.model += char(c);
        }
    }
    bms.last_update_ms = millis();
    Serial.printf("JBD model string: %s\n", bms.model.c_str());
    updateM365Data();
}

void updateM365Data() {
    if (!bms.valid) {
        return;
    }
    strncpy(g_M365BMS.serial, g_Settings.serial, sizeof(g_M365BMS.serial));
    g_M365BMS.design_capacity = uint16_t(min(65535, max(0, int(g_Settings.capacity))));
    g_M365BMS.real_capacity = g_M365BMS.design_capacity;
    int cellCount = max<int>(1, int(bms.cell_count));
    bool mapTo10 = g_Settings.map_to_10_cells && cellCount > 10;
    float mappedVoltage = bms.total_voltage;
    if (mapTo10) {
        float averageCell = bms.total_voltage / float(cellCount);
        mappedVoltage = averageCell * 10.0f;
    }
    int packNominal = g_Settings.nominal_voltage * (mapTo10 ? 10 : cellCount);
    g_M365BMS.nominal_voltage = uint16_t(min(65535, max(0, packNominal)));
    g_M365BMS.num_cycles = bms.cycles;
    g_M365BMS.num_charged = 0;
    g_M365BMS.max_voltage = uint16_t(min(65535, max(0, int(mappedVoltage * 100.0f))));
    g_M365BMS.max_discharge_current = uint16_t(min(65535, max(0, int(abs(bms.current_a) * 100.0f))));
    g_M365BMS.max_charge_current = g_M365BMS.max_discharge_current;
    g_M365BMS.date = uint16_t(((bms.production_year - 2000) << 9) | (bms.production_month << 5) | bms.production_day);
    g_M365BMS.status = bms.valid ? 1 : 0;
    g_M365BMS.capacity_left = uint16_t(min(65535, max(0, int(bms.residual_ah * 1000.0f))));
    g_M365BMS.percent_left = bms.rsoc;
    g_M365BMS.current = int16_t(max(-32768, min(32767, int(bms.current_a * 100.0f))));
    g_M365BMS.voltage = uint16_t(min(65535, max(0, int(mappedVoltage * 100.0f))));
    g_M365BMS.temperature[0] = bms.ntc_count_received > 0 ? uint8_t(max(0, min(255, int(bms.ntc_temps[0] + 20.0f)))) : 20;
    g_M365BMS.temperature[1] = bms.ntc_count_received > 1 ? uint8_t(max(0, min(255, int(bms.ntc_temps[1] + 20.0f)))) : 20;
    if (g_M365OvertempTest) {
        if (bms.ntc_count_received > 0) {
            g_M365BMS.temperature[0] = uint8_t(min(255, 130));
        }
        if (bms.ntc_count_received > 1) {
            g_M365BMS.temperature[1] = uint8_t(min(255, 130));
        }
    }
    g_M365BMS.balance_bits = uint16_t(bms.balance_low & 0xFFFFu);
    g_M365BMS.health = 100;
    for (int i = 0; i < 15; ++i) {
        if (mapTo10 && i >= 10) {
            g_M365BMS.cell_voltages[i] = 0;
        } else {
            g_M365BMS.cell_voltages[i] = (i < bms.cell_voltage_count) ? uint16_t(min(65535, max(0, int(bms.cell_voltages[i] * 1000.0f)))) : 0;
        }
    }
}

void m365Send(NinebotMessage &msg) {
    msg.checksum = msg.length + msg.addr + msg.mode + msg.offset;
    M365Serial.write(msg.header, 2);
    M365Serial.write(msg.length);
    M365Serial.write(msg.addr);
    M365Serial.write(msg.mode);
    M365Serial.write(msg.offset);
    for (uint8_t i = 0; i < msg.length - 2; ++i) {
        M365Serial.write(msg.data[i]);
        msg.checksum += msg.data[i];
    }
    msg.checksum ^= 0xFFFF;
    M365Serial.write(msg.checksum & 0xFF);
    M365Serial.write((msg.checksum >> 8) & 0xFF);
}

void onNinebotMessage(NinebotMessage &msg) {
    if (msg.addr != M365BMS_RADDR) {
        return;
    }

    if (msg.mode == 0x01 || msg.mode == 0xF1) {
        updateM365Data();
        uint16_t ofs = (uint16_t)msg.offset * 2;
        uint8_t sz = msg.data[0];
        msg.addr = M365BMS_WADDR;
        msg.length = 2 + sz;

        if (msg.mode == 0x01) {
            if ((ofs + sz) > sizeof(g_M365BMS)) {
                Serial.printf("[UART1] M365 read request out of range: ofs=0x%02X sz=%u\n", msg.offset, sz);
                return;
            }
            memcpy(&msg.data, &((uint8_t *)&g_M365BMS)[ofs], sz);
        } else {
            if ((ofs + sz) > sizeof(g_Settings)) {
                Serial.printf("[UART1] M365 settings request out of range: ofs=0x%02X sz=%u\n", msg.offset, sz);
                return;
            }
            memcpy(&msg.data, &((uint8_t *)&g_Settings)[ofs], sz);
        }

        Serial.printf("[UART1] Sending M365 reply mode=0x%02X offset=0x%02X len=%u\n", msg.mode, msg.offset, msg.length);
        Serial.print("[UART1] Reply data:");
        for (uint8_t i = 0; i < msg.length - 2; ++i) {
            Serial.printf(" %02X", msg.data[i]);
        }
        Serial.println();
        m365Send(msg);
    } else if (msg.mode == 0x03 || msg.mode == 0xF3) {
        uint16_t ofs = (uint16_t)msg.offset * 2;
        uint8_t sz = msg.length - 2;

        if (msg.mode == 0x03) {
            if ((ofs + sz) > sizeof(g_M365BMS)) {
                Serial.printf("[UART1] M365 write request out of range: ofs=0x%02X sz=%u\n", msg.offset, sz);
                return;
            }
            memcpy(&((uint8_t *)&g_M365BMS)[ofs], &msg.data, sz);
            Serial.printf("[UART1] Applied M365 BMS write ofs=0x%02X sz=%u\n", msg.offset, sz);
        } else {
            if ((ofs + sz) > sizeof(g_Settings)) {
                Serial.printf("[UART1] M365 settings write out of range: ofs=0x%02X sz=%u\n", msg.offset, sz);
                return;
            }
            memcpy(&((uint8_t *)&g_Settings)[ofs], &msg.data, sz);
            Serial.printf("[UART1] Applied M365 settings write ofs=0x%02X sz=%u\n", msg.offset, sz);
        }
    } else if (msg.mode == 0xFA) {
        switch (msg.offset) {
            case 1:
                updateM365Data();
                Serial.println("[UART1] M365 command: apply settings");
                break;
            case 2:
                memcpy(&g_Settings, &g_Settings, sizeof(g_Settings));
                Serial.println("[UART1] M365 command: reload settings");
                break;
            case 3:
                saveSettings();
                Serial.println("[UART1] M365 command: save settings");
                break;
            default:
                Serial.printf("[UART1] M365 unknown command 0xFA offset=0x%02X\n", msg.offset);
                break;
        }
    }
}

void processM365Serial() {
    static NinebotMessage msg;
    static uint8_t recvd = 0;
    static unsigned long begin = 0;
    static uint16_t checksum = 0;
    static int zero_silence = 0;

    while (M365Serial.available()) {
        if (millis() >= begin + 100) {
            recvd = 0;
            zero_silence = 0;
        }

        uint8_t byte = M365Serial.read();
        if (recvd == 0) {
            if (byte == 0x00) {
                zero_silence++;
                if (zero_silence == 1) {
                    Serial.println("[UART1] M365 RX is receiving 0x00; waiting for 0x55 header.");
                } else if (zero_silence == 100) {
                    Serial.println("[UART1] Still seeing 0x00 on M365 RX. Check wiring, baud, or that the controller is powered.");
                }
                continue;
            }
            zero_silence = 0;
            Serial.printf("[UART1] M365 byte received: 0x%02X\n", byte);
        }
        recvd++;

        switch (recvd) {
            case 1:
                if (byte != 0x55) {
                    recvd = 0;
                    break;
                }
                msg.header[0] = byte;
                begin = millis();
                break;
            case 2:
                if (byte != 0xAA) {
                    recvd = 0;
                    break;
                }
                msg.header[1] = byte;
                break;
            case 3:
                if (byte < 2) {
                    recvd = 0;
                    break;
                }
                msg.length = byte;
                checksum = byte;
                break;
            case 4:
                if (byte != M365BMS_RADDR) {
                    recvd = 0;
                    break;
                }
                msg.addr = byte;
                checksum += byte;
                break;
            case 5:
                msg.mode = byte;
                checksum += byte;
                break;
            case 6:
                msg.offset = byte;
                checksum += byte;
                break;
            default:
                if (recvd - 7 < msg.length - 2) {
                    msg.data[recvd - 7] = byte;
                    checksum += byte;
                } else if (recvd - 7 - msg.length + 2 == 0) {
                    msg.checksum = byte;
                } else {
                    msg.checksum |= (uint16_t)byte << 8;
                    checksum ^= 0xFFFF;
                    if (checksum == msg.checksum) {
                        Serial.printf("[UART1] Complete M365 packet: mode=0x%02X offset=0x%02X len=%u\n", msg.mode, msg.offset, msg.length);
                        Serial.print("[UART1] Packet data:");
                        for (uint8_t i = 0; i < msg.length - 2; ++i) {
                            Serial.printf(" %02X", msg.data[i]);
                        }
                        Serial.println();
                        onNinebotMessage(msg);
                    } else {
                        Serial.printf("[UART1] M365 packet checksum failed: expected=0x%04X got=0x%04X\n", checksum, msg.checksum);
                    }
                    recvd = 0;
                }
                break;
        }
    }
}

void process_uart_data() {
    while (JbdSerial.available()) {
        int b = JbdSerial.read();
        if (b < 0) {
            return;
        }
        if (frame_len == 0) {
            if (b != 0xDD) {
                continue;
            }
            frame_buf[frame_len++] = uint8_t(b);
            continue;
        }
        frame_buf[frame_len++] = uint8_t(b);
        if (frame_len >= sizeof(frame_buf)) {
            frame_len = 0;
            continue;
        }
        if (frame_len >= 4) {
            uint8_t declared_len = frame_buf[3];
            size_t full_len = 4 + declared_len + 3;
            if (frame_len == full_len) {
                if (frame_buf[frame_len - 1] != 0x77) {
                    Serial.printf("Frame end mismatch: expected 0x77, got 0x%02X\n", frame_buf[frame_len - 1]);
                    print_raw_frame(frame_buf, frame_len);
                    frame_len = 0;
                    continue;
                }
                uint8_t cmd = frame_buf[1];
                switch (cmd) {
                    case 0x03:
                        decode_basic_info(frame_buf, frame_len);
                        break;
                    case 0x04:
                        decode_cell_voltages(frame_buf, frame_len);
                        break;
                    case 0x05:
                        decode_model_string(frame_buf, frame_len);
                        break;
                    default:
                        print_raw_frame(frame_buf, frame_len);
                        break;
                }
                frame_len = 0;
            } else if (frame_len > full_len) {
                Serial.printf("Frame length overrun: got %u, expected %u\n", frame_len, full_len);
                print_raw_frame(frame_buf, frame_len);
                frame_len = 0;
            }
        }
    }
}

void send_query(const uint8_t *data, size_t len, const char *name) {
    JbdSerial.write(data, len);
    Serial.printf(">>> JBD query: %s\n", name);
}

String buildStatusJson() {
    uint16_t protection_override = bms.protection;
    bool overtemp_test = g_M365OvertempTest;
    if (overtemp_test) {
        if (bms.current_a < 0.0f) {
            protection_override |= (1u << 6);
        } else {
            protection_override |= (1u << 4);
        }
    }
    String protection_text = overtemp_test ? format_protection_text(protection_override) : bms.protection_text;

    String json = "{";
    json += "\"valid\":" + String(bms.valid ? "true" : "false");
    json += ",\"total_voltage\":" + String(bms.total_voltage, 2);
    json += ",\"current_a\":" + String(bms.current_a, 2);
    json += ",\"residual_ah\":" + String(bms.residual_ah, 2);
    json += ",\"nominal_ah\":" + String(bms.nominal_ah, 2);
    json += ",\"rsoc\":" + String(bms.rsoc);
    json += ",\"fet\":" + String(bms.fet);
    json += ",\"cell_count\":" + String(bms.cell_count);
    json += ",\"ntc_count\":" + String(bms.ntc_count);
    json += ",\"cycles\":" + String(bms.cycles);
    json += ",\"production_year\":" + String(bms.production_year);
    json += ",\"production_month\":" + String(bms.production_month);
    json += ",\"production_day\":" + String(bms.production_day);
    json += ",\"protection\":" + String(protection_override);
    json += ",\"balance_low\":" + String(bms.balance_low);
    json += ",\"balance_high\":" + String(bms.balance_high);
    json += ",\"protection_text\":\"" + protection_text + "\"";
    json += ",\"balance_text\":\"" + bms.balance_text + "\"";
    json += ",\"model\":\"" + bms.model + "\"";
    json += ",\"last_update_ms\":" + String(bms.last_update_ms);
    json += ",\"cell_voltages\":[";
    for (int i = 0; i < bms.cell_voltage_count; ++i) {
        json += String(bms.cell_voltages[i], 3);
        if (i + 1 < bms.cell_voltage_count) json += ",";
    }
    json += "],\"ntc_temps\":[";
    for (int i = 0; i < bms.ntc_count_received; ++i) {
        float tempValue = bms.ntc_temps[i];
        if (overtemp_test) {
            tempValue = 110.0f;
        }
        json += String(tempValue, 1);
        if (i + 1 < bms.ntc_count_received) json += ",";
    }
    json += "]";
    json += "}";
    return json;
}
