# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

An ESPHome firmware for an ESP32 that emulates a **Victron VM-3P75CT** energy meter on RS-485/Modbus RTU. The Victron GX device (Venus OS, `dbus-cgwacs` driver) polls the ESP32 as if it were a real Carlo Gavazzi EM24 DINAV53 meter. Energy readings are pulled from Home Assistant sensor entities via the ESPHome native API and written into Modbus holding registers in real time.

The meter role (grid, PV inverter, EV charger, etc.) is configured in the Victron GX UI under **Settings → Energy Meters** after the ESP32 is connected.

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

- **`modbus_address`** — Modbus slave address; must match what the Victron GX is configured to poll (default `1`).
- **`ha_l*_*`** — Home Assistant entity IDs for each measurement. Units must be: voltage in V, current in A, power in W (positive = import), frequency in Hz, energy in kWh (cumulative).

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
  Modbus holding registers  (in-memory, inside esphome-modbus-server component)
        │  (RS-485, Modbus RTU FC 0x03, 9600 8N1)
        ▼
  Victron GX device  →  Venus OS D-Bus  →  VRM / MPPT / inverter control
```

### Modbus register layout (Carlo Gavazzi EM24)

The GX performs two bulk reads per poll cycle:

| FC03 request | Registers | Contents |
|---|---|---|
| First | 0–51 | Voltages, currents, power per phase, total power, frequency |
| Second | 52–79 | Energy import/export totals |
| ID check | 4096 | Device type — must return `0x000B` (EM24 DINAV53) |

Key register addresses (all values in holding register space):

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
| 4096 | Device type | UINT16 | fixed `0x000B` |

INT32 encoding: high 16-bit word at the lower register address (big-endian word order).

### External component

`epiclabs-io/esphome-modbus-server` (GitHub) provides the Modbus RTU slave. Key API used in lambdas:
- `id(victron_meter)->write_holding_register(addr, uint16_t value)`
- `id(victron_meter)->read_holding_register(addr)` → `uint16_t`

### Hardware wiring

```
ESP32           MAX485 module       Victron GX RS-485 port
GPIO17 (TX) →  DI
GPIO16 (RX) →  RO
GPIO4       →  DE + RE (bridged)
               A+              →   A+
               B-              →   B-
               VCC             ←   +5 V (from GX, pin 1)
               GND             ←   GND (from GX, pin 2)
```

The Victron GX RS-485 Energy Meter port pinout (RJ12 or screw terminal depending on model): pin 1 = +5 V, pin 2 = GND, pin 3 = A+, pin 4 = B−.

## Debugging Tips

- Use `esphome logs` and watch for `[modbus_server]` lines to confirm incoming FC 0x03 requests from the GX.
- To test registers independently, use `mbpoll` on a Linux machine with a USB-RS485 adapter: `mbpoll -a 1 -r 0 -c 52 /dev/ttyUSB0` should return all measurement registers.
- If the GX does not discover the meter, check: correct slave address, correct A+/B− polarity, and that register 4096 returns `0x000B`.
