# VM-3P75CT Native Victron Modbus Register Map

The ESP32 emulates a **Victron Energy VM-3P75CT** (product ID `0xa1b1`) using the
native Victron register map defined in
[`dbus-modbus-client/victron_em.py`](https://github.com/victronenergy/dbus-modbus-client/blob/master/victron_em.py)
(class `VE_Meter_A1B1`).

> **Important:** This is NOT the Carlo Gavazzi EM24 register map. The VM-3P75CT
> uses its own native Victron protocol. Always refer to this document and
> `victron_em.py` — do not use EM24 documentation.

## Physical layer

| Parameter | Value |
|-----------|-------|
| Protocol | Modbus TCP |
| Port | 502 |
| Unit ID | 1 (default) |
| Function codes | FC 0x03 (Read Holding Registers), FC 0x04 (Read Input Registers) |

## Identification registers (written at boot)

| Reg (hex) | Value | Description |
|-----------|-------|-------------|
| 0x1000 | 0xa1b1 | Product ID — VM-3P75CT |
| 0x1009 | 0x0001 | Firmware version high (major.minor = 0.1) |
| 0x100a | 0x0900 | Firmware version low (patch.build = 9.0) |
| 0x100b | 0x0001 | Hardware version |
| 0x2000 | 0x0003 | Phase config: 3 = three-phase L1+L2+L3 |
| 0x2001 | 0x0000 | Role: 0 = grid meter |
| 0x3038 | 0x0000 | Error code: 0 = no error |

The Cerbo GX reads `0x1000` to confirm the device is a VM-3P75CT before
accepting it. Firmware version ≥ 0.1.9.0 enables all data paths (current,
line-to-line voltage, power factor).

## Register encoding

All multi-word values use **big-endian word order**: the high 16-bit word is at
the lower register address.

| Type | Size | Description |
|------|------|-------------|
| s16 | 1 reg | Signed 16-bit integer |
| s32b | 2 regs | Signed 32-bit, big-endian word order |
| u16 | 1 reg | Unsigned 16-bit integer |
| u32b | 2 regs | Unsigned 32-bit, big-endian word order |

## Total registers

| Reg (hex) | Type | Scale | Unit | Description |
|-----------|------|-------|------|-------------|
| 0x3032 | u16 | ÷100 | Hz | Frequency |
| 0x3034–35 | u32b | ÷100 | kWh | Total energy forward (import) |
| 0x3036–37 | u32b | ÷100 | kWh | Total energy reverse (export) |
| 0x3038 | u16 | — | — | Error code |
| 0x3080–81 | s32b | ÷1 | W | Total active power (sum of phases) |

## Per-phase registers

Phase base address: `base = 0x3040 + 8 × (n−1)`
Phase power address: `power = 0x3082 + 4 × (n−1)`

| Phase | Voltage (s16 ÷100 V) | Current (s16 ÷100 A) | L-L Voltage (s16 ÷100 V) | Energy Fwd (u32b ÷100 kWh) | Energy Rev (u32b ÷100 kWh) | Power (s32b ÷1 W) |
|-------|----------------------|----------------------|--------------------------|-----------------------------|-----------------------------|-------------------|
| L1 | 0x3040 | 0x3041 | 0x3046 | 0x3042–43 | 0x3044–45 | 0x3082–83 |
| L2 | 0x3048 | 0x3049 | 0x304e | 0x304a–4b | 0x304c–4d | 0x3086–87 |
| L3 | 0x3050 | 0x3051 | 0x3056 | 0x3052–53 | 0x3054–55 | 0x308a–8b |

## Lambda scaling summary

| Sensor (HA unit) | Register value written |
|------------------|----------------------|
| Voltage V | `(int16_t)(x * 100)` |
| Current A | `(int16_t)(x * 100)` |
| Power W | `(int32_t)(x)` — integer watts, no scaling |
| Frequency Hz | `(uint16_t)(x * 100)` |
| Energy kWh | `(uint32_t)(x * 100)` |
