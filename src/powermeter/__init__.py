import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor,text_sensor, number
from esphome.const import *

AUTO_LOAD = ['sensor','text_sensor']

powermeter_sensor_ns = cg.esphome_ns.namespace("powermeter_sensor")
PowerMeterComponent = powermeter_sensor_ns.class_(
    "PowerMeterComponent", cg.PollingComponent
)

ChannelState = powermeter_sensor_ns.enum("CHANNEL_STATE")
CHANNEL_STATES = {
    "UNKNOWN": ChannelState.UNKNOWN,
    "IDLE": ChannelState.IDLE,
    "DISCONNECTED": ChannelState.DISCONNECTED,
    "ACTIVE": ChannelState.ACTIVE,
    "ERROR": ChannelState.ERROR,    
}

CONF_VOLTAGE_CALIBRATION = "voltage_calibration"

CONFIG_SCHEMA = cv.Schema({
    cv.GenerateID(): cv.declare_id(PowerMeterComponent),
    cv.Optional(CONF_VOLTAGE): 
        sensor.sensor_schema(
            device_class=DEVICE_CLASS_VOLTAGE,
            unit_of_measurement=UNIT_VOLT,
            accuracy_decimals=2,
            state_class=STATE_CLASS_MEASUREMENT
        )
}).extend(
    cv.Schema(
        {
            cv.Optional(f"{CONF_CHANNEL}_{i + 1}"): cv.Schema(
                {
                    cv.Optional(CONF_CURRENT): 
                        cv.maybe_simple_value(
                            sensor.sensor_schema(
                                device_class=DEVICE_CLASS_CURRENT,
                                unit_of_measurement=UNIT_AMPERE,
                                accuracy_decimals=2,
                                state_class=STATE_CLASS_MEASUREMENT
                            ),
                            key=CONF_NAME
                        ),
                    cv.Optional(CONF_POWER): 
                        cv.maybe_simple_value(
                            sensor.sensor_schema(
                                device_class=DEVICE_CLASS_POWER,
                                unit_of_measurement=UNIT_WATT,
                                accuracy_decimals=2,
                                state_class=STATE_CLASS_MEASUREMENT
                            ),
                            key=CONF_NAME,
                        ),
                    cv.Optional(CONF_APPARENT_POWER): 
                        cv.maybe_simple_value(
                            sensor.sensor_schema(
                                device_class=DEVICE_CLASS_APPARENT_POWER,
                                unit_of_measurement=UNIT_VOLT_AMPS,
                                accuracy_decimals=2,
                                state_class=STATE_CLASS_MEASUREMENT
                            ),
                            key=CONF_NAME,
                        ),
                    cv.Optional(CONF_ENERGY):
                        cv.maybe_simple_value(
                            sensor.sensor_schema(
                                device_class=DEVICE_CLASS_ENERGY,
                                unit_of_measurement=UNIT_WATT_HOURS,
                                accuracy_decimals=2,
                                state_class=STATE_CLASS_TOTAL_INCREASING
                            ),
                            key=CONF_NAME,
                        ),
                    cv.Optional(CONF_STATE): 
                        cv.maybe_simple_value(
                            text_sensor.text_sensor_schema(
                                entity_category=ENTITY_CATEGORY_NONE
                            ),
                            key=CONF_NAME,
                        )
                }
            )
            for i in range(6)
        }
    )
).extend(cv.polling_component_schema("60s"))


def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    yield cg.register_component(var, config)
    
    if global_voltage_config := config.get(CONF_VOLTAGE):
        sens = yield sensor.new_sensor(global_voltage_config)
        cg.add(var.set_global_voltage_sensor(sens))

#    if voltage_calibration := config.get(CONF_VOLTAGE_CALIBRATION):
#        numb = yield number.new_number(voltage_calibration, min_value=0, max_value=100, step=1)
#        yield cg.register_component(numb, voltage_calibration)
#        cg.add(var.set_voltage_calibration(numb))
    
    
    for i in range(6):
        ch = i + 1
        if channel_config := config.get(f"{CONF_CHANNEL}_{ch}"):
            if current_config := channel_config.get(CONF_CURRENT):
                sens = yield sensor.new_sensor(current_config)
                cg.add(var.set_Irms_sensor(ch, sens))
            if apparent_power_config := channel_config.get(CONF_APPARENT_POWER):
                sens = yield sensor.new_sensor(apparent_power_config)
                cg.add(var.set_Papp_sensor(ch, sens))
            if real_power_config := channel_config.get(CONF_POWER):
                sens = yield sensor.new_sensor(real_power_config)
                cg.add(var.set_Preal_sensor(ch, sens))
            if energy_config := channel_config.get(CONF_ENERGY):
                sens = yield sensor.new_sensor(energy_config)
                cg.add(var.set_activeEnergy_sensor(ch, sens))
            if state_config := channel_config.get(CONF_STATE):
                sens = yield text_sensor.new_text_sensor(state_config)
                cg.add(var.set_state_sensor(ch, sens))

