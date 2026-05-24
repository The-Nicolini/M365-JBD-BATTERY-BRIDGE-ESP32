# Contributing

Thanks for your interest. This is a small hardware/firmware project, so contributions are welcome but scope is intentionally narrow.

## Ways to contribute

- **Bug reports** — use the [bug report template](.github/ISSUE_TEMPLATE/bug_report.md)
- **Feature requests** — use the [feature request template](.github/ISSUE_TEMPLATE/feature_request.md)
- **Code improvements** — fork → branch → PR (details below)

## Hardware prerequisites

To test firmware changes you need:

- ESP32-S3 dev board (DevKitC-1 or compatible)
- JBD BMS (SP04S / SP14S series recommended)
- M365 ESC or a serial monitor to simulate ESC requests
- PlatformIO (VS Code extension or CLI)

## Development workflow

```bash
# Fork on GitHub, then:
git clone https://github.com/<your-username>/M365-JBD-BATTERY-BRIDGE-ESP32.git
cd M365-JBD-BATTERY-BRIDGE-ESP32

# Build
pio run

# Flash
pio run --target upload --upload-port <your-port>

# Monitor
pio device monitor --port <your-port> --baud 115200
```

## Pull request checklist

- [ ] Firmware builds with no errors (`pio run`)
- [ ] Flash usage stays well under board limits
- [ ] No changes to default SSID/password constants — users configure via settings modal
- [ ] If you change the `/data` JSON schema, update the JS in `HTML_PAGE` too
- [ ] If you add a new setting, add it to both `buildSettingsJson()` and `handleSettings()`
- [ ] Describe what hardware you tested on (BMS model, S-count, ESC type)

## Code style

- Arduino/C++ in `src/main.cpp` — keep it single-file for simplicity
- 2-space indentation, no trailing whitespace
- `static` on all module-level symbols
- Comment protocol magic numbers (register addresses, packet bytes)

## What is out of scope

- Replacing PlatformIO / Arduino framework with IDF
- Adding OTA update support (security implications on an AP device need careful thought)
- Supporting non-ESP32 platforms
