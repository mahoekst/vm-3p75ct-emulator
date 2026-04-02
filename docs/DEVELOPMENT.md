# Development

## Project structure

```
vm-3p75ct-emulator/
├── esphome/
│   ├── vm-3p75ct-emulator.yaml        # Production config — paste into HA
│   ├── vm-3p75ct-emulator-dev.yaml    # Local dev config — uses local component
│   ├── secrets.yaml.example           # Template (committed)
│   ├── secrets.yaml                   # Your credentials (git-ignored)
│   └── components/
│       └── modbus_tcp_server/
│           ├── __init__.py            # ESPHome schema + code generation
│           ├── modbus_tcp_server.h    # C++ class declaration
│           └── modbus_tcp_server.cpp  # Modbus TCP server implementation
└── docs/
```

## ESPHome commands

Run these from the repo root, or `cd esphome/` first and drop the `esphome/` path prefix.

```bash
# Validate config — no device needed, catches YAML and C++ errors
esphome compile esphome/vm-3p75ct-emulator-dev.yaml

# Compile + flash over USB
esphome upload esphome/vm-3p75ct-emulator-dev.yaml

# Compile + flash + open serial monitor in one step
esphome run esphome/vm-3p75ct-emulator-dev.yaml

# Stream logs over WiFi (device must already be on the network)
esphome logs esphome/vm-3p75ct-emulator-dev.yaml

# Generate a new API encryption key
esphome gen-api-key
```

## Dev vs production YAML

| | `vm-3p75ct-emulator-dev.yaml` | `vm-3p75ct-emulator.yaml` |
|---|---|---|
| Component source | Local `components/` directory | GitHub (fetched automatically) |
| Logger level | `DEBUG` | `INFO` |
| Use case | Local iteration on C++ component | Paste into Home Assistant |

Always develop against the `*-dev.yaml`. Only push to GitHub when ready, then HA will pick up the update on the next compile.

## Testing registers

Use `mbpoll` from any machine on the same LAN as the ESP32:

```bash
# Read measurement registers 0–51
mbpoll -a 1 -r 0 -c 52 <ESP32_IP>

# Read energy registers 52–79
mbpoll -a 1 -r 52 -c 28 <ESP32_IP>

# Read device ID register (must return 11 = 0x000B)
mbpoll -a 1 -r 4096 -c 1 <ESP32_IP>
```

## Custom component internals

The `modbus_tcp_server` component (`esphome/components/modbus_tcp_server/`) implements a minimal Modbus TCP server:

- Listens on port 502 (configurable)
- Handles one TCP connection at a time, multiple requests per connection (2 s window)
- Supports FC 0x03 (Read Holding Registers) only — the only function code the Victron GX uses
- Registers stored in a `std::map<uint16_t, uint16_t>` — sparse, zero pre-allocation overhead
- Public API: `write_holding_register(addr, value)` / `read_holding_register(addr)`
