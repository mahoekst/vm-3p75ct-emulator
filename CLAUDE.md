# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

An ESPHome firmware for an ESP32 that emulates a **Victron VM-3P75CT** energy meter over **Modbus TCP** (WiFi). The Victron GX device (Venus OS) polls the ESP32 on port 502 as if it were a real Carlo Gavazzi EM24 DINAV53 meter. Energy readings are pulled from Home Assistant sensor entities via the ESPHome native API and written into Modbus holding registers in real time. No RS-485 hardware needed.

The meter role (grid, PV inverter, EV charger, etc.) is configured in the Victron GX UI under **Settings → Energy Meters** after pointing it at the ESP32's IP address.

## ESPHome Commands

```bash
# Validate config (no device needed)
esphome compile vm-3p75ct-emulator.yaml

# Compile + flash over USB
esphome upload vm-3p75ct-emulator.yaml

# Compile + flash + open serial monitor
esphome run vm-3p75ct-emulator.yaml

# Stream logs over WiFi (after first flash)
esphome logs vm-3p75ct-emulator.yaml

# Generate an API encryption key
esphome gen-api-key
```

## Configuration

All user-facing configuration lives in the `substitutions:` block at the top of `vm-3p75ct-emulator.yaml`:

- **`modbus_unit_id`** — Modbus unit ID the GX uses when polling (default `1`).
- **`ha_l*_*`** — Home Assistant entity IDs for each measurement. Units: voltage in V, current in A, power in W (positive = import), frequency in Hz, energy in kWh (cumulative).

WiFi credentials and API key live in `secrets.yaml` (not committed).

## Architecture

### Data flow

```
Home Assistant sensors
        │  (ESPHome native API, push on change)
        ▼
  on_value lambdas
        │  (scale + encode as INT32/UINT32/INT16)
        ▼
  Modbus holding registers  (in-memory std::map, inside modbus_tcp_server component)
        │  (TCP port 502, Modbus TCP FC 0x03)
        ▼
  Victron GX device  →  Venus OS D-Bus  →  VRM / MPPT / inverter control
```

### Custom component — `components/modbus_tcp_server/`

A minimal Modbus TCP server written as a local ESPHome external component. Implements FC 0x03 (Read Holding Registers) only — the only function code the Victron GX uses for meter polling.

| File | Purpose |
|------|---------|
| `__init__.py` | ESPHome config schema + code generation |
| `modbus_tcp_server.h` | C++ class declaration |
| `modbus_tcp_server.cpp` | TCP server loop, MBAP frame parsing, FC03 response |

Registers are stored in a `std::map<uint16_t, uint16_t>` — sparse, no pre-allocation needed.

Key API used in sensor lambdas:
- `id(victron_meter)->write_holding_register(addr, uint16_t value)`
- `id(victron_meter)->read_holding_register(addr)` → `uint16_t`

### Modbus register layout (Carlo Gavazzi EM24)

The GX performs two bulk FC03 reads per poll cycle plus an ID check:

| FC03 request | Registers | Contents |
|---|---|---|
| First | 0–51 | Voltages, currents, power per phase, total power, frequency |
| Second | 52–79 | Energy import/export totals |
| ID check | 4096 | Device type — must return `0x000B` (EM24 DINAV53) |

Register map:

| Reg | Description | Encoding | Scale |
|-----|-------------|----------|-------|
| 0–1 | V L1-N | INT32 | ÷10 → V |
| 2–3 | V L2-N | INT32 | ÷10 → V |
| 4–5 | V L3-N | INT32 | ÷10 → V |
| 6–7 | V L1-L2 | INT32 | ÷10 → V |
| 8–9 | V L2-L3 | INT32 | ÷10 → V |
| 10–11 | V L3-L1 | INT32 | ÷10 → V |
| 12–13 | I L1 | INT32 | ÷1000 → A |
| 14–15 | I L2 | INT32 | ÷1000 → A |
| 16–17 | I L3 | INT32 | ÷1000 → A |
| 18–19 | P L1 | INT32 | ÷10 → W |
| 20–21 | P L2 | INT32 | ÷10 → W |
| 22–23 | P L3 | INT32 | ÷10 → W |
| 40–41 | Total P | INT32 | ÷10 → W (auto-sum of L1+L2+L3) |
| 51 | Frequency | INT16 | ÷10 → Hz |
| 52–53 | Energy import | UINT32 | ÷10 → Wh (HA kWh × 10000) |
| 78–79 | Energy export | UINT32 | ÷10 → Wh (HA kWh × 10000) |
| 4096 | Device type | UINT16 | fixed `0x000B` — set at boot |

INT32 encoding: high 16-bit word at the lower register address (big-endian word order).

### Victron GX configuration

Venus OS connects to Modbus TCP energy meters via the `dbus-modbus-client` service. Add the ESP32 as a Carlo Gavazzi / AC sensor device at its LAN IP address, port 502, unit ID 1. On newer Venus OS versions this can be done from the GX touch UI; on older versions it may require SSH and editing `/etc/dbus-modbus-client.conf`.

## Debugging Tips

- `esphome logs` — watch for `[modbus_tcp]` lines confirming incoming FC03 requests from the GX.
- Test with `mbpoll` (Linux): `mbpoll -a 1 -r 0 -c 52 <ESP32_IP>` — should return all measurement registers.
- If the GX ignores the meter, verify register 4096 returns `0x000B` and that the IP/port/unit ID match the GX configuration.
