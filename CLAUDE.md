# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

An ESPHome firmware for an ESP32 that emulates a **Carlo Gavazzi EM24 DINAV53** energy meter over **Modbus TCP** (WiFi, port 502). The Victron GX device polls the ESP32 using the EM24 register map (model ID `1651`, defined in `dbus-modbus-client/carlo_gavazzi.py`). Energy readings come from Home Assistant sensor entities via the ESPHome native API.

> **Important:** This project emulates the **Carlo Gavazzi EM24** register map (little-endian s32l, ÷10 voltage/power, ÷1000 current). Do NOT use the native Victron VM-3P75CT register map — it is completely different.

## ESPHome Commands

All commands run from the repo root. ESPHome is not on PATH — use `python3 -m esphome`. Use `em24-emulator-dev.yaml` for local work.

```bash
python3 -m esphome compile esphome/em24-emulator-dev.yaml   # validate only
python3 -m esphome run     esphome/em24-emulator-dev.yaml   # compile + flash + monitor
python3 -m esphome upload  esphome/em24-emulator-dev.yaml   # compile + flash (no monitor)
python3 -m esphome logs    esphome/em24-emulator-dev.yaml   # stream logs over WiFi
python3 -m esphome gen-api-key                              # generate API encryption key
```

## Project Structure

```
esphome/
├── em24-emulator.yaml          # Production — paste into Home Assistant
├── em24-emulator-dev.yaml      # Local dev — uses local component path
├── secrets.yaml.example        # Committed template
├── secrets.yaml                # Git-ignored; copy from .example and fill in
└── components/
    └── modbus_tcp_server/
        ├── __init__.py         # ESPHome config schema + code generation
        ├── modbus_tcp_server.h # C++ class declaration
        └── modbus_tcp_server.cpp # Modbus TCP server implementation
docs/
├── QUICKSTART.md
├── CONFIGURATION.md
├── DEVELOPMENT.md
└── REGISTER_MAP.md
```

## Dev vs Production YAML

| File | Component source | Logger |
|------|-----------------|--------|
| `em24-emulator-dev.yaml` | `type: local, path: components` | `DEBUG` |
| `em24-emulator.yaml` | `type: git, url: github.com/mahoekst/em24-emulator` | `INFO` |

## Architecture

```
Home Assistant sensors
        │  (ESPHome native API, push on change)
        ▼
  modbus_tcp_server typed setters  (set_voltage, set_current, set_power, ...)
        ▼
  std::map<uint16_t, uint16_t>  (sparse register map in component)
        │  (Modbus TCP FC 0x03 / FC 0x04, port 502)
        ▼
  Victron GX  →  Venus OS D-Bus  →  VRM / inverter control
```

## Custom Component API

`esphome/components/modbus_tcp_server/` — local ESPHome external component.

Typed setters (preferred — called from generated HA sensor subscriptions):
- `set_voltage(phase, volts)` — L-N voltage, s32l ÷10
- `set_current(phase, amps)` — current, s32l ÷1000
- `set_power(phase, watts)` — active power, s32l ÷10; total auto-updated
- `set_frequency(hz)` — u16 ÷10
- `set_energy_import(kwh)` / `set_energy_export(kwh)` — total, s32l ÷10
- `set_phase_energy_import(phase, kwh)` — per-phase, s32l ÷10

Low-level access:
- `write_holding_register(uint16_t addr, uint16_t value)`
- `read_holding_register(uint16_t addr) → uint16_t`

Identification registers (model ID, serial, phase config) are written in `setup()` automatically.

## Key Register Addresses (EM24 map)

See [docs/REGISTER_MAP.md](docs/REGISTER_MAP.md) for the full table. Critical ones:

| Reg (hex) | Description | Encoding |
|-----------|-------------|----------|
| 0x000B | Model ID = 1651 | EM24DINAV53XE1X |
| 0x1002 | Phase config | 0 = 3-phase, 3 = 1-phase |
| 0x0000/02/04 | V L1-N, L2-N, L3-N | s32l ÷10 → V |
| 0x000c/0e/10 | I L1, L2, L3 | s32l ÷1000 → A |
| 0x0012/14/16 | P L1, L2, L3 | s32l ÷10 → W |
| 0x0028 | Total power | s32l ÷10 → W (auto-summed) |
| 0x0033 | Frequency | u16 ÷10 → Hz |
| 0x0034/004e | Energy import/export | s32l ÷10 → kWh |
| 0x0040/42/44 | Energy import L1/L2/L3 | s32l ÷10 → kWh |
| 0xa000 | Application mode | fixed 7 (mode H) |
| 0xa100 | Switch position | fixed 3 (Locked) |
