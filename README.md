# M365 JBD Battery Bridge — ESP32-S3

Replaces the stock Xiaomi M365 BMS with a custom lithium pack managed by a **JBD (Jiabaida) BMS**.  
An **ESP32-S3** sits in the middle, translating between the two protocols and hosting a **live WiFi battery dashboard**.

---

## How it works

```
┌──────────────┐   UART 9600    ┌─────────────┐   UART 115200   ┌──────────────┐
│  JBD BMS     │◄──────────────►│  ESP32-S3   │◄───────────────►│  M365 ESC    │
│ (SP14S004    │  DD/A5 protocol│             │  55 AA protocol  │              │
│  or similar) │                │  WiFi AP    │                  │ (motor ctrl) │
└──────────────┘                │  Dashboard  │                  └──────────────┘
                                └─────────────┘
                                       ▲
                                       │ HTTP
                                  Phone / Laptop
                                  connected to AP
```

1. ESP32 polls the JBD BMS over UART every 500 ms (configurable) — reads voltage, current, SoC, cell voltages, temperatures, and protection flags.
2. It answers M365 ESC requests in the native M365 protocol, forwarding battery state.  
   Voltage is linearly mapped from the real pack's S-count range to the stock M365 10S range (30–42 V) so the ESC always sees a valid pack.
3. A WiFi Access Point (`M365-BMS` / `m365batt` by default) hosts a web dashboard at `192.168.4.1`.  
   If no client connects within **2 minutes** of boot, the AP shuts down and the bridge keeps running silently.

---

## Hardware

| Part | Notes |
|---|---|
| ESP32-S3-DevKitC-1 | Any ESP32-S3 board works; adjust pins if needed |
| JBD BMS | Tested target: JBD-SP14S004 (7–14S configurable) |
| DC–DC level shifter | If BMS UART is 5 V logic; ESP32-S3 is 3.3 V |
| Power supply | Power ESP32 from the pack's 3.3 V / 5 V rail or a small buck converter |

---

## Wiring

```
JBD BMS                    ESP32-S3                   M365 ESC
─────────────────────────────────────────────────────────────────
BMS TX ──────────────────► GPIO 16 (JBD_RX)
BMS RX ◄────────────────── GPIO 17 (JBD_TX)
                           GPIO 18 (M365_RX) ◄──────── ESC TX
                           GPIO 19 (M365_TX) ──────────► ESC RX
GND ──────────────────────── GND ──────────────────── GND
```

> **⚠ Voltage warning:** The JBD UART signals may be 5 V. Use a level shifter or voltage divider on BMS TX → GPIO 16.  
> **⚠ Power warning:** If your custom pack voltage differs from the stock 36 V, ensure the M365 ESC power input is within its rated range. The firmware only handles *communication* voltage mapping — it cannot protect hardware from overvoltage.

### M365 battery connector pinout (JST-style, 4-wire)

| Pin | Signal |
|---|---|
| 1 | Pack + (36 V nominal) |
| 2 | UART TX (ESC → BMS) |
| 3 | UART RX (BMS → ESC) |
| 4 | GND |

Connect pins 2 & 3 to ESP32 GPIO 18 & 19 respectively (cross TX↔RX).

---

## Build & Flash

### Requirements
- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- ESP32-S3-DevKitC-1 connected via USB (CH343 / CP210x driver)

### Steps

```bash
# Clone
git clone https://github.com/The-Nicolini/M365-JBD-BATTERY-BRIDGE-ESP32.git
cd M365-JBD-BATTERY-BRIDGE-ESP32

# Build
pio run

# Flash (adjust port as needed)
pio run --target upload --upload-port COM4   # Windows
pio run --target upload --upload-port /dev/ttyUSB0  # Linux
```

`platformio.ini` already sets `upload_port = COM4` — change it to match your system.

---

## Web Dashboard

1. On your phone or laptop, connect to WiFi:  
   **SSID:** `M365-BMS`  **Password:** `m365batt`
2. Open a browser and go to **`http://192.168.4.1`**

### Dashboard shows

- Pack voltage, current, SoC %
- Remaining / nominal capacity
- Per-cell voltages with colour coding
- NTC temperatures
- Protection fault flags (over-voltage, under-voltage, over-current, short, etc.)
- Connection status dot (green = live data, red = JBD not responding)

### Quick controls

| Control | Function |
|---|---|
| ⚡ ESC Kill | Reports SoC = 0 and voltage = 0 to the ESC → motor disabled instantly |

---

## Settings (⚙ gear icon)

All settings are applied immediately and **persisted to NVS flash** (survive reboots) where noted.

### WiFi AP *(persisted)*
| Field | Description |
|---|---|
| Network Name (SSID) | AP name broadcast by the ESP32 |
| Password | Min 8 characters. Leave blank to keep current. Changing restarts the AP. |

### BMS Protocol
JBD / Overkill Solar selected. DALY and JIKONG support planned.

### BMS Control
| Field | Description |
|---|---|
| Bluetooth | Sends a write to the JBD BT register to enable/disable the onboard BT module |
| BT Register | Hex register address for BT control (default `E1` — verify with logic analyser for your firmware) |
| Poll interval | How often the ESP32 queries the JBD: 200 ms / 500 ms / 1 s / 2 s |

### ESC Control
| Field | Description |
|---|---|
| Pack Cells (S) | Number of series cells in your pack (7–16 S). Used to map real pack voltage to the M365 10S range. |
| ESC Kill | Toggle motor disable — ESC receives SoC = 0, voltage = 0 |
| SoC Override | Report a fixed SoC % to the ESC (useful for speed-limiting) |

---

## Voltage mapping

The M365 ESC expects a 10S Li-ion pack (30 V empty → 42 V full).  
If your pack has a different S count, the firmware maps linearly:

$$V_{ESC} = 30\,\text{V} + \frac{V_{actual} - N_S \times 3.0\,\text{V}}{N_S \times 4.2\,\text{V} - N_S \times 3.0\,\text{V}} \times 12\,\text{V}$$

Set **Pack Cells** in settings to match your pack.

---

## AP auto-shutdown

If no WiFi client connects within **2 minutes** of power-on, the AP and web server are disabled to save radio power. The JBD↔M365 bridge continues running on the last-saved settings.

To re-enable WiFi: power-cycle or reset the ESP32.

---

## Protocol reference

### JBD UART (9600 8N1)
- **Read:** `DD A5 <cmd> 00 <crc_hi> <crc_lo> 77`
- **Response:** `DD <cmd> 00 <len> <data…> <crc_hi> <crc_lo> 77`
- **Write:** `DD 5A <reg> <len> <data…> <crc_hi> <crc_lo> 77`
- **CRC:** `(0x10000 − sum_of_data_bytes) & 0xFFFF`
- Cmd `0x03` = basic info, `0x04` = cell voltages

### M365 UART (115200 8N1)
- **Packet:** `55 AA <pkt_len> <src> <dst> <cmd> [data…] <crc_lo> <crc_hi>`
- **CRC:** `(~sum) & 0xFFFF` little-endian
- ESC address `0x21`, BMS address `0x22`

---

## License

MIT — do what you want, no warranty.
