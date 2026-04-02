# Quick Start

## Prerequisites

- ESP32 development board (generic `esp32dev`)
- [ESPHome](https://esphome.io/guides/installing_esphome) installed locally
- Home Assistant with ESPHome add-on
- Victron GX device (Cerbo GX, CCGX, Venus GX, etc.) on the same LAN

## 1. Copy secrets

```bash
cp esphome/secrets.yaml.example esphome/secrets.yaml
```

Edit `esphome/secrets.yaml` with your WiFi credentials and a generated API key:

```bash
esphome gen-api-key   # paste the output as api_encryption_key
```

## 2. Configure entity IDs

Open `esphome/vm-3p75ct-emulator.yaml` and update the `substitutions:` block with your actual Home Assistant entity IDs for each phase measurement.

See [CONFIGURATION.md](CONFIGURATION.md) for details on each field.

## 3. Flash the ESP32 (first time, USB)

```bash
esphome run esphome/vm-3p75ct-emulator-dev.yaml
```

After the first flash, subsequent updates can be done over WiFi (OTA).

## 4. Add to Home Assistant

Paste the contents of `esphome/vm-3p75ct-emulator.yaml` into a new device in the Home Assistant ESPHome dashboard. HA will fetch the component from GitHub automatically.

## 5. Configure Victron GX

In Venus OS:
1. Make sure the ESP32 and GX are on the same LAN
2. Go to **Settings → Energy Meters** (or use `dbus-modbus-client` configuration via SSH)
3. Add a new meter: type = Carlo Gavazzi, IP = ESP32's IP address, port = 502, unit ID = 1
4. Set the meter **role** (grid meter, PV inverter, EV charger) as needed
5. The GX will start polling and data will appear in the overview
