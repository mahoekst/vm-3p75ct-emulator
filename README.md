# Carlo Gavazzi EM24 Emulator

An ESPHome firmware for an ESP32 that emulates a **Carlo Gavazzi EM24 DINAV53** three-phase energy meter over **Modbus TCP** (WiFi). The Victron GX device polls the ESP32 on TCP port 502 as if it were a real EM24 meter.

Energy readings are pulled from **Home Assistant sensor entities** via the ESPHome native API and mapped in real time into the EM24 Modbus register layout.

## Features

- Emulates Carlo Gavazzi EM24 DINAV53 — identified by the Victron GX as a native energy meter
- Modbus TCP on port 502 — connects over WiFi, no extra hardware
- Configurable 1-phase or 3-phase mode
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
