# Changelog

All notable changes to this project will be documented here.  
Format based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/).

---

## [1.0.0] — 2026-05-24

### Added
- JBD BMS UART polling (9600 8N1, GPIO 16/17) — basic info + cell voltages
- M365 ESC UART bridge (115200 8N1, GPIO 18/19) — answers status requests and pushes unsolicited packets
- Linear voltage mapping from real pack S-count range to M365 10S range (30–42 V)
- WiFi softAP (`M365-BMS` / `m365batt` default) with web dashboard at `192.168.4.1`
- Live battery dashboard: pack voltage, current, SoC, per-cell voltages, temperatures, protection flags
- Settings modal (gear icon) with runtime configuration — no reflash needed
- BT toggle — sends JBD write to enable/disable onboard Bluetooth module
- ESC Kill — reports SoC = 0, voltage = 0 to disable motor instantly
- SoC override — spoof a fixed SoC percentage to the ESC
- Configurable poll interval (200 ms / 500 ms / 1 s / 2 s)
- Pack cell count setting (7–16 S) for correct voltage mapping
- WiFi SSID/password management with NVS persistence (survives reboots)
- AP auto-shutdown after 2 minutes with no connected client
- Modal field freeze during editing (prevents 1-second poll from overwriting typed values)
