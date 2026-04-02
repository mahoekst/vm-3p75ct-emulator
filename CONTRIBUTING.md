# Contributing

## Development setup

1. Clone the repo and `cd` into it
2. `cp esphome/secrets.yaml.example esphome/secrets.yaml` and fill in your values
3. Edit component code in `esphome/components/modbus_tcp_server/`
4. Compile and flash with the dev config: `esphome run esphome/vm-3p75ct-emulator-dev.yaml`

See [docs/DEVELOPMENT.md](docs/DEVELOPMENT.md) for the full workflow.

## Pull requests

- Keep changes focused — one feature or fix per PR
- Test against a real Victron GX device or verify registers with `mbpoll` before submitting
- Update `docs/REGISTER_MAP.md` if the register mapping changes
