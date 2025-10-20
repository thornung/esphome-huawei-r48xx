import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import button
from esphome.const import CONF_ENTITY_CATEGORY, ENTITY_CATEGORY_CONFIG, CONF_ID

from .. import HuaweiR48xxComponent, huawei_r48xx_ns, CONF_HUAWEI_R48xx_ID

HuaweiR48xxButton = huawei_r48xx_ns.class_(
    "HuaweiR48xxButton", button.Button, cg.Component
)

CONF_SET_OFFLINE_VALUES = "set_offline_values"

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_HUAWEI_R48xx_ID): cv.use_id(HuaweiR48xxComponent),
            cv.Optional(CONF_SET_OFFLINE_VALUES): button.button_schema(
                class_=HuaweiR48xxButton,
                entity_category=ENTITY_CATEGORY_CONFIG,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)

async def to_code(config):
    hub = await cg.get_variable(config[CONF_HUAWEI_R48xx_ID])
    if config.get(CONF_SET_OFFLINE_VALUES):
        conf = config[CONF_SET_OFFLINE_VALUES]
        var = cg.new_Pvariable(conf[CONF_ID])
        await cg.register_component(var, conf)
        await button.register_button(var, conf)
        cg.add(var.set_parent(hub))
