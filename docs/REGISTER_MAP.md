# EM24 Modbus Register Map

The VM-3P75CT is an OEM of the **Carlo Gavazzi EM24 DINAV53D2X**. The Victron GX driver (`dbus-cgwacs`) polls using the EM24 register layout below.

## Physical layer

| Parameter | Value |
|-----------|-------|
| Protocol | Modbus TCP (this emulator) / Modbus RTU on real device |
| Port | 502 |
| Unit ID | 1 (default) |
| Function code | FC 0x03 — Read Holding Registers |

## GX poll pattern

The GX issues two bulk FC03 reads per cycle:

| Request | Registers | Contents |
|---------|-----------|----------|
| 1 | 0–51 | Voltages, currents, powers, power factor, frequency |
| 2 | 52–79 | Energy totals |
| ID check | 4096 | Device type (once on startup) |

## Register encoding

All multi-byte values use **big-endian word order**: the high 16-bit word is at the lower register address.

- **INT32** — signed 32-bit integer in two consecutive registers
- **UINT32** — unsigned 32-bit integer in two consecutive registers
- **INT16** — signed 16-bit integer in a single register

## Voltage

| Reg (dec) | Description | Type | Scale | Unit |
|-----------|-------------|------|-------|------|
| 0–1 | Voltage L1-N | INT32 | ÷10 | V |
| 2–3 | Voltage L2-N | INT32 | ÷10 | V |
| 4–5 | Voltage L3-N | INT32 | ÷10 | V |
| 6–7 | Voltage L1-L2 | INT32 | ÷10 | V |
| 8–9 | Voltage L2-L3 | INT32 | ÷10 | V |
| 10–11 | Voltage L3-L1 | INT32 | ÷10 | V |

## Current

| Reg | Description | Type | Scale | Unit |
|-----|-------------|------|-------|------|
| 12–13 | Current L1 | INT32 | ÷1000 | A |
| 14–15 | Current L2 | INT32 | ÷1000 | A |
| 16–17 | Current L3 | INT32 | ÷1000 | A |

## Active power

| Reg | Description | Type | Scale | Unit |
|-----|-------------|------|-------|------|
| 18–19 | Active Power L1 | INT32 | ÷10 | W |
| 20–21 | Active Power L2 | INT32 | ÷10 | W |
| 22–23 | Active Power L3 | INT32 | ÷10 | W |
| 40–41 | Total Active Power | INT32 | ÷10 | W |

Positive = import (load consuming), negative = export (feed-in to grid).

## Reactive and apparent power (not mapped — registers return 0)

| Reg | Description | Type | Scale | Unit |
|-----|-------------|------|-------|------|
| 24–25 | Reactive Power L1 | INT32 | ÷10 | VAr |
| 26–27 | Reactive Power L2 | INT32 | ÷10 | VAr |
| 28–29 | Reactive Power L3 | INT32 | ÷10 | VAr |
| 30–31 | Apparent Power L1 | INT32 | ÷10 | VA |
| 32–33 | Apparent Power L2 | INT32 | ÷10 | VA |
| 34–35 | Apparent Power L3 | INT32 | ÷10 | VA |
| 42–43 | Total Reactive Power | INT32 | ÷10 | VAr |
| 46–47 | Total Apparent Power | INT32 | ÷10 | VA |

## Power factor and frequency

| Reg | Description | Type | Scale | Unit |
|-----|-------------|------|-------|------|
| 44 | Power Factor L1 | INT16 | ÷1000 | — |
| 45 | Power Factor L2 | INT16 | ÷1000 | — |
| 46 | Power Factor L3 | INT16 | ÷1000 | — |
| 47 | Total Power Factor | INT16 | ÷1000 | — |
| 51 | Frequency | INT16 | ÷10 | Hz |

## Energy

Register unit is **0.1 Wh** (scale ÷10 → Wh). HA sensors in kWh are multiplied by 10 000.

| Reg | Description | Type | Scale | Unit |
|-----|-------------|------|-------|------|
| 52–53 | Total Energy Import | UINT32 | ÷10 | Wh |
| 54–55 | Energy Import L1 | UINT32 | ÷10 | Wh |
| 56–57 | Energy Import L2 | UINT32 | ÷10 | Wh |
| 58–59 | Energy Import L3 | UINT32 | ÷10 | Wh |
| 78–79 | Total Energy Export | UINT32 | ÷10 | Wh |

## Device identification

| Reg | Description | Value |
|-----|-------------|-------|
| 4096 | Device type | `0x000B` = EM24 DINAV53 |

The GX reads this register once on startup to confirm it is talking to a Carlo Gavazzi EM24 device. It is pre-populated at boot via the ESPHome `on_boot` automation.
