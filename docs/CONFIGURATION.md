# Configuration

All user-facing settings live in the `substitutions:` block at the top of the YAML files. Edit them once and the rest of the config stays untouched.

## Substitution variables

| Variable | Default | Description |
|----------|---------|-------------|
| `device_name` | `vm-3p75ct-emulator` | ESPHome device name (also the mDNS hostname) |
| `modbus_unit_id` | `1` | Modbus unit ID the Victron GX uses when polling. Must match GX configuration. |

## Home Assistant entity IDs

Each variable maps to an HA sensor entity that provides the corresponding measurement. Replace the placeholder values with your actual entity IDs.

| Variable | Unit | Description |
|----------|------|-------------|
| `ha_l1_voltage` | V | Phase 1 voltage (L1-N) |
| `ha_l2_voltage` | V | Phase 2 voltage (L2-N) |
| `ha_l3_voltage` | V | Phase 3 voltage (L3-N) |
| `ha_l1l2_voltage` | V | Line voltage L1-L2 |
| `ha_l2l3_voltage` | V | Line voltage L2-L3 |
| `ha_l3l1_voltage` | V | Line voltage L3-L1 |
| `ha_l1_current` | A | Phase 1 current |
| `ha_l2_current` | A | Phase 2 current |
| `ha_l3_current` | A | Phase 3 current |
| `ha_l1_power` | W | Phase 1 active power (positive = import, negative = export) |
| `ha_l2_power` | W | Phase 2 active power |
| `ha_l3_power` | W | Phase 3 active power |
| `ha_frequency` | Hz | Grid frequency |
| `ha_energy_import` | kWh | Cumulative total energy imported |
| `ha_energy_export` | kWh | Cumulative total energy exported |

## Secrets (`esphome/secrets.yaml`)

Copy `esphome/secrets.yaml.example` to `esphome/secrets.yaml` (git-ignored) and fill in:

| Key | Description |
|-----|-------------|
| `wifi_ssid` | WiFi network name |
| `wifi_password` | WiFi password |
| `api_encryption_key` | ESPHome API key — generate with `esphome gen-api-key` |
| `ota_password` | Password for over-the-air firmware updates |

## Meter role

The meter role (grid, PV inverter, EV charger, generator) is **not** configured in the YAML — it is set in the Victron GX UI under **Settings → Energy Meters** after the device is connected.
