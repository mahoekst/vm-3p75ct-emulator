# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

An ESPHome firmware for an ESP32 that emulates a **Victron VM-3P75CT** energy meter over **Modbus TCP** (WiFi, port 502). The Victron GX device polls the ESP32 using the **native Victron register map** (product ID `0xa1b1`, defined in `dbus-modbus-client/victron_em.py`). Energy readings come from Home Assistant sensor entities via the ESPHome native API.

> **Important:** This project uses the **native Victron VM-3P75CT register map**, NOT the Carlo Gavazzi EM24 map. Do not confuse the two — they are completely different register addresses and scaling factors.

## ESPHome Commands

All commands run from the repo root. ESPHome is not on PATH — use `python3 -m esphome`. Use `vm-3p75ct-emulator-dev.yaml` for local work.

```bash
python3 -m esphome compile esphome/vm-3p75ct-emulator-dev.yaml   # validate only
python3 -m esphome run     esphome/vm-3p75ct-emulator-dev.yaml   # compile + flash + monitor
python3 -m esphome upload  esphome/vm-3p75ct-emulator-dev.yaml   # compile + flash (no monitor)
python3 -m esphome logs    esphome/vm-3p75ct-emulator-dev.yaml   # stream logs over WiFi
python3 -m esphome gen-api-key                                    # generate API encryption key
```

## Project Structure

```
esphome/
├── vm-3p75ct-emulator.yaml         # Production — paste into Home Assistant
├── vm-3p75ct-emulator-dev.yaml     # Local dev — uses local component path
├── secrets.yaml.example            # Committed template
├── secrets.yaml                    # Git-ignored; copy from .example and fill in
└── components/
    └── modbus_tcp_server/
        ├── __init__.py             # ESPHome config schema + code generation
        ├── modbus_tcp_server.h     # C++ class declaration
        └── modbus_tcp_server.cpp   # Modbus TCP server implementation
docs/
├── QUICKSTART.md
├── CONFIGURATION.md
├── DEVELOPMENT.md
└── REGISTER_MAP.md
```

## Dev vs Production YAML

The only difference between the two YAML files is the `external_components` source:

| File | Component source | Logger |
|------|-----------------|--------|
| `*-dev.yaml` | `type: local, path: components` | `DEBUG` |
| `*.yaml` | `type: git, url: github.com/mahoekst/vm-3p75ct-emulator` | `INFO` |

All substitutions and sensor lambdas are identical. Edit the substitutions block in both files when adding/renaming entity IDs.

## Architecture

```
Home Assistant sensors
        │  (ESPHome native API, push on change)
        ▼
  on_value lambdas  (scale + encode INT32/UINT32/INT16)
        ▼
  std::map<uint16_t, uint16_t>  (in modbus_tcp_server component)
        │  (Modbus TCP FC 0x03, port 502)
        ▼
  Victron GX  →  Venus OS D-Bus  →  VRM / inverter control
```

## Custom Component API

`esphome/components/modbus_tcp_server/` — local ESPHome external component.

- `write_holding_register(uint16_t addr, uint16_t value)` — called from sensor lambdas
- `read_holding_register(uint16_t addr) → uint16_t`
- Handles FC 0x03 and FC 0x04; registers stored in a sparse `std::map`
- Identification registers set at boot via `on_boot:` in the YAML

## Key Register Addresses

See [docs/REGISTER_MAP.md](docs/REGISTER_MAP.md) for the full table. Critical ones:

| Reg (hex) | Description | Encoding |
|-----------|-------------|----------|
| 0x1000 | Product ID | fixed `0xa1b1` (VM-3P75CT) |
| 0x2000 | Phase config | fixed `0x0003` (three-phase) |
| 0x2001 | Role | fixed `0x0000` (grid) |
| 0x3040, 0x3048, 0x3050 | V L1-N, L2-N, L3-N | s16 ÷100 → V |
| 0x3046, 0x304e, 0x3056 | V L1-L2, L2-L3, L3-L1 | s16 ÷100 → V |
| 0x3041, 0x3049, 0x3051 | I L1, L2, L3 | s16 ÷100 → A |
| 0x3082–83, 0x3086–87, 0x308a–8b | P L1, L2, L3 | s32b ÷1 → W |
| 0x3080–81 | Total P (auto-summed) | s32b ÷1 → W |
| 0x3032 | Frequency | u16 ÷100 → Hz |
| 0x3034–35, 0x3036–37 | Energy import/export | u32b ÷100 → kWh (HA kWh × 100) |
