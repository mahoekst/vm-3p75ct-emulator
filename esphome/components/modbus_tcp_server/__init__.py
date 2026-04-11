import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT
from esphome.core import CORE

CONF_UNIT_ID = "unit_id"
CONF_PHASES = "phases"
CONF_SERIAL_NUMBER = "serial_number"

# ── HA entity_id fields — all optional ────────────────────────────────────────
# Each key maps to a typed setter on ModbusTcpServer:
#   l1_voltage  → set_voltage(1, x)   etc.
#
# (key, typed setter call with placeholder `x` for the float value)
_SENSOR_DEFS = [
    ("l1_voltage",    "set_voltage(1, x)"),
    ("l2_voltage",    "set_voltage(2, x)"),
    ("l3_voltage",    "set_voltage(3, x)"),
    ("l1l2_voltage",  "set_ll_voltage(1, x)"),
    ("l2l3_voltage",  "set_ll_voltage(2, x)"),
    ("l3l1_voltage",  "set_ll_voltage(3, x)"),
    ("l1_current",    "set_current(1, x)"),
    ("l2_current",    "set_current(2, x)"),
    ("l3_current",    "set_current(3, x)"),
    ("l1_power",      "set_power(1, x)"),
    ("l2_power",      "set_power(2, x)"),
    ("l3_power",      "set_power(3, x)"),
    ("frequency",     "set_frequency(x)"),
    ("energy_import", "set_energy_import(x)"),
    ("energy_export", "set_energy_export(x)"),
]

modbus_tcp_server_ns = cg.esphome_ns.namespace("modbus_tcp_server")
ModbusTcpServer = modbus_tcp_server_ns.class_("ModbusTcpServer", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ModbusTcpServer),
            cv.Optional(CONF_PORT, default=502): cv.port,
            cv.Optional(CONF_UNIT_ID, default=1): cv.int_range(min=1, max=247),
            cv.Optional(CONF_PHASES, default=3): cv.one_of(1, 3, int=True),
            cv.Optional(CONF_SERIAL_NUMBER, default="00000000000000"): cv.All(
                cv.string, cv.Length(min=1, max=14)
            ),
            # Optional HA entity_id for each measurement — when present, a
            # homeassistant state subscription is generated automatically.
            **{cv.Optional(key): cv.string for key, _ in _SENSOR_DEFS},
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_unit_id(config[CONF_UNIT_ID]))
    cg.add(var.set_phases(config[CONF_PHASES]))
    cg.add(var.set_serial_number(config[CONF_SERIAL_NUMBER]))
    cg.add_library("WiFi", None)

    # ── Generate HA entity subscriptions ──────────────────────────────────────
    # For each configured entity_id, emit a subscribe_home_assistant_state()
    # call that forwards the float value to the appropriate typed setter.
    # This replaces the explicit sensor: platform: homeassistant blocks and
    # their lambda register-write code in the YAML.
    server_id = config[CONF_ID].id

    for key, method_call in _SENSOR_DEFS:
        if key not in config:
            continue
        entity_id = config[key]
        # Generates (in main.cpp setup()):
        #   api::global_api_server->subscribe_home_assistant_state(
        #       "sensor.foo", {}, [](const std::string &state) {
        #           auto v = parse_number<float>(state);
        #           if (v.has_value()) <server_id>->set_voltage(1, *v);
        #       });
        stmt = (
            f'api::global_api_server->subscribe_home_assistant_state('
            f'"{entity_id}", {{}}, '
            f'[](const std::string &__state, const std::string &__type) {{'
            f'  auto __v = parse_number<float>(__state);'
            f'  if (__v.has_value()) {server_id}->{method_call.replace("x", "*__v")};'
            f'}})'
        )
        cg.add(cg.RawExpression(stmt))

    if any(key in config for key, _ in _SENSOR_DEFS):
        cg.add_global_include("esphome/components/api/api_server.h")
        cg.add_global_include("esphome/core/helpers.h")
