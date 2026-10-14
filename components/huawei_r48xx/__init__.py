import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components.canbus import CanbusComponent
from esphome.const import CONF_ID

CONF_CANBUS_ID = "canbus_id"
CONF_MEASUREMENT_SCALE = "measurement_scale"
CONF_HUAWEI_R48xx_ID = "huawei_r48xx_id"

huawei_r48xx_ns = cg.esphome_ns.namespace("huawei_r48xx")
HuaweiR48xxComponent = huawei_r48xx_ns.class_(
    "HuaweiR48xxComponent", cg.PollingComponent
)

CONFIG_SCHEMA = cv.Schema(
    {
        cv.GenerateID(): cv.declare_id(HuaweiR48xxComponent),
        cv.Required(CONF_CANBUS_ID): cv.use_id(CanbusComponent),
        # Korrekturfaktor fuer gemeldeten Strom/Leistung (Register 0x70/0x72/0x73/0x81).
        #   Zahl : fester Faktor (R4850: 1.0 = Default, R4875G1: 0.512)
        #   auto : Geraetetyp per CAN-Info-Abfrage erkennen und Faktor aus der Typ-Tabelle nehmen
        # Die Erkennung laeuft immer und schreibt Typ/Nennstrom ins Log.
        cv.Optional(CONF_MEASUREMENT_SCALE, default=1.0): cv.Any(
            cv.one_of("auto", lower=True), cv.float_range(min=0.05, max=20.0)
        ),
    }
).extend(cv.polling_component_schema("5s"))


async def to_code(config):
    canbus = await cg.get_variable(config[CONF_CANBUS_ID])
    var = cg.new_Pvariable(config[CONF_ID], canbus)
    await cg.register_component(var, config)
    scale = config[CONF_MEASUREMENT_SCALE]
    if scale == "auto":
        cg.add(var.set_measurement_scale_auto(True))
    else:
        cg.add(var.set_measurement_scale(scale))
