# M365 JBD Battery Bridge — ESP32-S3 / ESP8266 D1 Mini

Bridges a custom lithium pack (managed by a **JBD (Jiabaida) BMS**) to the Xiaomi M365 ESC.  
Supports both **ESP32-S3** and **ESP8266 D1 Mini** variants. The board taps into the M365 serial bus alongside the stock BLE module, translating between the two protocols and hosting a **live WiFi battery dashboard**.

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
   The ESP32 is powered from the scooter's **5 V rail** (only live when the scooter is switched on), so the AP starts automatically on every ride.  
   If no client connects within **2 minutes** of power-on, the AP shuts down — this is intentional: the bridge keeps running but radio is off, and you cannot accidentally leave a WiFi network broadcasting indefinitely.

---

## Hardware

| Part | Notes |
|---|---|
| ESP32-S3-DevKitC-1 | Any ESP32-S3 board works; adjust pins if needed |
| ESP8266 D1 Mini | Supported for lightweight installations; see pin mapping below |
| JBD BMS | Tested target: JBD-SP14S004 (7–14S configurable) |
| DC–DC level shifter | If BMS UART is 5 V logic; ESP32/ESP8266 are 3.3 V |
| 3.3 V LDO / buck converter | Power board from the scooter's 5 V rail (see wiring) |
| 2× resistors ~680 Ω–1 kΩ | Series protection on GPIO RX/TX pins |
| 1× small signal diode | Half-duplex collision protection on M365 bus (see below) |

---

## Wiring

### M365 BLE module connector (4-wire, as labelled on the module)

The M365 runs a **single-wire (half-duplex) serial bus** between the BLE module and ESC at 115200 bps 8N1.  
The ESP32 **taps into this bus alongside the stock BLE module** — the BLE module remains in place and still functions. The ESP32 listens and responds to ESC requests independently.

| Wire colour | Label | Signal |
|---|---|---|
| Black | G | Ground |
| Yellow | T | One-wire serial bus (115200 bps, 8N1) |
| Green | P | VBatt — always available (pack voltage, ~36 V) |
| Red | 5 | 5 V — only present when scooter is switched on |

> **Power the board** from the **5 V (red)** rail through a 3.3 V LDO or small buck converter.  
> This rail is only active when the scooter is switched on — so the board powers up and starts its 2-minute WiFi window automatically on every ride.  
> The ESP32-S3 GPIO pins are 5 V tolerant, so no level shifter is needed for the serial line on ESP32-S3. For ESP8266 D1 Mini, use a level shifter or voltage divider on BMS TX → D5.

### M365 serial bus connection

The yellow `T` wire is a **shared one-wire bus** — RX and TX are the same physical line.  
Use the following protection circuit to avoid bus contention:

```
                      680R–1kΩ
M365 "T" wire ────────┬──/\/\/──── ESP32 GPIO 18 (M365_RX)
                      │
                    [diode, anode→bus]
                      │  100–200R
                      └──/\/\/──── ESP32 GPIO 19 (M365_TX)
```

- Series resistor (680 Ω–1 kΩ) on the RX path protects the GPIO
- Diode + 100–200 Ω resistor from RX toward TX prevents TX from back-driving RX while listening

### JBD BMS connection (standard full-duplex UART)

```
JBD BMS                    ESP32-S3 / ESP8266 D1 Mini
──────────────────────────────────────────────────────
BMS TX ──────────────────► GPIO 16 (JBD_RX)  or D5 on D1 Mini
BMS RX ◄────────────────── GPIO 17 (JBD_TX)  or D6 on D1 Mini
GND ──────────────────────── GND

For the ESP8266 D1 Mini, use the following board pins:
- JBD_RX = D5
- JBD_TX = D6
- M365_RX = D7
- M365_TX = D1

> **⚠ Boot note:** D1 Mini `D8` / GPIO15 is not used by this firmware because it is a boot-strapping pin.
```

> **⚠ Voltage warning:** JBD UART signals may be 5 V. Use a level shifter or voltage divider on BMS TX → GPIO 16 or D5.  
> **⚠ Power warning:** The firmware handles *communication* voltage mapping only — ensure the ESC power input stays within its rated range for your pack voltage.

---

## Build & Flash

### Requirements
- [PlatformIO](https://platformio.org/) (VS Code extension or CLI)
- ESP32-S3-DevKitC-1 or ESP8266 D1 Mini connected via USB (CH343 / CP210x driver)

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

---

## Status

> **⚠ Not yet physically tested.**  
> The firmware compiles and the web dashboard works over WiFi, but the full JBD ↔ ESP32 ↔ M365 ESC chain has not been validated on real hardware at time of writing.  
> Hardware testing is in progress — this notice will be removed once confirmed working.
