# Victron VM-3P75CT Emulator

An ESPHome firmware for an ESP32 that emulates a **Victron VM-3P75CT** three-phase energy meter over **Modbus TCP** (WiFi). The Victron GX device polls the ESP32 on TCP port 502 as if it were a real Carlo Gavazzi EM24 DINAV53 meter, with no RS-485 hardware required.

Energy readings are pulled from **Home Assistant sensor entities** via the ESPHome native API and mapped in real time into the EM24 Modbus register layout.

## Features

- Emulates Carlo Gavazzi EM24 DINAV53 — the OEM device behind the Victron VM-3P75CT
- Modbus TCP on port 502 — connects over WiFi, no extra hardware
- Full three-phase support: voltage, current, active power, frequency, import/export energy
- Configurable meter role (grid, PV inverter, EV charger) via the Victron GX UI
- OTA firmware updates and Home Assistant integration via ESPHome native API

## Quick Start

See [docs/QUICKSTART.md](docs/QUICKSTART.md).

## Documentation

| Document | Description |
|----------|-------------|
| [docs/QUICKSTART.md](docs/QUICKSTART.md) | First-time setup from zero to running |
| [docs/CONFIGURATION.md](docs/CONFIGURATION.md) | All substitution variables and HA entity IDs |
| [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) | Local build, compile, flash, and test workflow |
| [docs/REGISTER_MAP.md](docs/REGISTER_MAP.md) | Full EM24 Modbus register map |

## License

MIT — see [LICENSE](LICENSE).
