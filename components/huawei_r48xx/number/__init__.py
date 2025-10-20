import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import number
from esphome.const import (
    UNIT_VOLT,
    CONF_ID,
    CONF_ICON,
    CONF_UNIT_OF_MEASUREMENT,
    CONF_MODE,
    CONF_ENTITY_CATEGORY,
    ICON_FLASH,
    ICON_CURRENT_AC,
    CONF_MIN_VALUE,
    CONF_MAX_VALUE,
    CONF_STEP,
    UNIT_AMPERE,
    ENTITY_CATEGORY_NONE,
)

from .. import HuaweiR48xxComponent, huawei_r48xx_ns, CONF_HUAWEI_R48xx_ID

CONF_OUTPUT_VOLTAGE = "output_voltage"
CONF_MAX_OUTPUT_CURRENT = "max_output_current"

HuaweiR48xxNumber = huawei_r48xx_ns.class_(
    "HuaweiR48xxNumber", number.Number, cg.Component
)

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_HUAWEI_R48xx_ID): cv.use_id(HuaweiR48xxComponent),
            cv.Optional(CONF_OUTPUT_VOLTAGE): number.number_schema(
                class_=HuaweiR48xxNumber,
                min_value=42,
                max_value=56,
                step=0.1,
                icon=ICON_FLASH,
                unit_of_measurement=UNIT_VOLT,
                mode="BOX",
                entity_category=ENTITY_CATEGORY_NONE,
            ),
            cv.Optional(CONF_MAX_OUTPUT_CURRENT): number.number_schema(
                class_=HuaweiR48xxNumber,
                min_value=0,
                max_value=45,
                step=0.1,
                icon=ICON_CURRENT_AC,
                unit_of_measurement=UNIT_AMPERE,
                mode="BOX",
                entity_category=ENTITY_CATEGORY_NONE,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)

async def to_code(config):
    hub = await cg.get_variable(config[CONF_HUAWEI_R48xx_ID])
    if config.get(CONF_OUTPUT_VOLTAGE):
        conf = config[CONF_OUTPUT_VOLTAGE]
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)
        await number.register_number(
            var,
            conf,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
        )
        cg.add(getattr(hub, "set_output_voltage_number")(var))
        cg.add(var.set_parent(hub, 0x0))
    if config.get(CONF_MAX_OUTPUT_CURRENT):
        conf = config[CONF_MAX_OUTPUT_CURRENT]
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)
        await number.register_number(
            var,
            conf,
            min_value=conf[CONF_MIN_VALUE],
            max_value=conf[CONF_MAX_VALUE],
            step=conf[CONF_STEP],
        )
        cg.add(getattr(hub, "set_max_output_current_number")(var))
        cg.add(var.set_parent(hub, 0x3))
