import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.const import CONF_ID, CONF_PORT
from esphome.core import CORE

CONF_UNIT_ID = "unit_id"

modbus_tcp_server_ns = cg.esphome_ns.namespace("modbus_tcp_server")
ModbusTcpServer = modbus_tcp_server_ns.class_("ModbusTcpServer", cg.Component)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(ModbusTcpServer),
            cv.Optional(CONF_PORT, default=502): cv.port,
            cv.Optional(CONF_UNIT_ID, default=1): cv.int_range(min=1, max=247),
        }
    ).extend(cv.COMPONENT_SCHEMA),
    cv.only_on_esp32,
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)
    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_unit_id(config[CONF_UNIT_ID]))
    cg.add_library("WiFi", None)
