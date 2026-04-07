# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

An ESPHome firmware for an ESP32 that emulates a **Victron VM-3P75CT** energy meter over **Modbus TCP** (WiFi, port 502). The Victron GX device polls the ESP32 as if it were a real Carlo Gavazzi EM24 DINAV53 meter. Energy readings come from Home Assistant sensor entities via the ESPHome native API.

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
- Handles FC 0x03 only; registers stored in a sparse `std::map`
- Device ID register 4096 = `0x000B` set at boot via `on_boot:` in the YAML

## Key Register Addresses

See [docs/REGISTER_MAP.md](docs/REGISTER_MAP.md) for the full table. Critical ones:

| Reg | Description | Encoding |
|-----|-------------|----------|
| 0–5 | V L1-N, L2-N, L3-N | INT32 ÷10 → V |
| 6–11 | V L1-L2, L2-L3, L3-L1 | INT32 ÷10 → V |
| 12–17 | I L1, L2, L3 | INT32 ÷1000 → A |
| 18–23 | P L1, L2, L3 | INT32 ÷10 → W |
| 40–41 | Total P (auto-summed) | INT32 ÷10 → W |
| 51 | Frequency | INT16 ÷10 → Hz |
| 52–53, 78–79 | Energy import/export | UINT32 ÷10 → Wh (HA kWh × 10 000) |
| 4096 | Device type | fixed `0x000B` |
