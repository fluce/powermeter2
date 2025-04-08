#pragma once

#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/text_sensor/text_sensor.h"

#include "types.h"
extern channel_t channels[];
extern channel_t channel_standard_voltage;

namespace esphome {
namespace powermeter_sensor {

enum CHANNEL_STATE: uint8_t {
  UNKNOWN = 0,
  IDLE = 1,
  DISCONNECTED = 2,
  ACTIVE = 3,
  ERROR = 4
};

class PowerMeterChannel
{
 public:
  PowerMeterChannel(uint8_t index, channel_t* channel) : index(),channel(channel) {}

  void set_Vrms_sensor(sensor::Sensor *sensor) { this->Vrms_sensor = sensor; }
  void set_Irms_sensor(sensor::Sensor *sensor) { this->Irms_sensor = sensor; }
  void set_Papp_sensor(sensor::Sensor *sensor) { this->Papp_sensor = sensor; }
  void set_Preal_sensor(sensor::Sensor *sensor) { this->Preal_sensor = sensor; }
  void set_power_factor_sensor(sensor::Sensor *sensor) { this->power_factor_sensor = sensor; }
  void set_phase_sensor(sensor::Sensor *sensor) { this->phase_sensor = sensor; }
  void set_activeEnergy_sensor(sensor::Sensor *sensor) { this->activeEnergy_sensor = sensor; }
  void set_reactiveEnergy_sensor(sensor::Sensor *sensor) { this->reactiveEnergy_sensor = sensor; }
  void set_timestamp_sensor(sensor::Sensor *sensor) { this->timestamp_sensor = sensor; }
  void set_state_sensor(text_sensor::TextSensor *sensor) { this->state_sensor = sensor; }

  void publish_state() {
    if (this->channel->measure.state == MEASURE_STATE_ACTIVE) {
      if (this->Vrms_sensor != nullptr) this->Vrms_sensor->publish_state(this->channel->measure.Vrms);
      if (this->Irms_sensor != nullptr) this->Irms_sensor->publish_state(this->channel->measure.Irms);
      if (this->Papp_sensor != nullptr) this->Papp_sensor->publish_state(this->channel->measure.Papp);
      if (this->Preal_sensor != nullptr) this->Preal_sensor->publish_state(this->channel->measure.Preal);
      if (this->power_factor_sensor != nullptr) this->power_factor_sensor->publish_state(this->channel->measure.power_factor);
      if (this->phase_sensor != nullptr) this->phase_sensor->publish_state(this->channel->measure.phase);
      if (this->activeEnergy_sensor != nullptr) this->activeEnergy_sensor->publish_state(this->channel->index.activeEnergyIndex);
      if (this->reactiveEnergy_sensor != nullptr) this->reactiveEnergy_sensor->publish_state(this->channel->index.reactiveEnergyIndex);
    }
    if (this->timestamp_sensor != nullptr) this->timestamp_sensor->publish_state(this->channel->measure.timestamp);
    if (this->state_sensor != nullptr) {
      char* state = "UNKNOWN";
      switch (this->channel->measure.state) {
        case MEASURE_STATE_IDLE: state = "IDLE"; break;
        case MEASURE_STATE_OPEN: state = "DISCONNECTED"; break;
        case MEASURE_STATE_ACTIVE: state = "ACTIVE"; break;
        case MEASURE_STATE_ERROR: state = "ERROR"; break;
        default: break;
      }
      this->state_sensor->publish_state(state);
    }
  }

  /*void set_calibration_ratio(number::Number *calibration_ratio) {
    this->calibration_ratio = calibration_ratio;
  }*/

 protected:
  uint8_t index;
  channel_t* channel = nullptr;
  sensor::Sensor *Vrms_sensor = nullptr;
  sensor::Sensor *Irms_sensor = nullptr;
  sensor::Sensor *Papp_sensor = nullptr;
  sensor::Sensor *Preal_sensor = nullptr;
  sensor::Sensor *power_factor_sensor = nullptr;
  sensor::Sensor *phase_sensor = nullptr;
  sensor::Sensor *activeEnergy_sensor = nullptr;
  sensor::Sensor *reactiveEnergy_sensor = nullptr;
  sensor::Sensor *timestamp_sensor = nullptr;
  text_sensor::TextSensor *state_sensor = nullptr;

  //number::Number *calibration_ratio = nullptr;

};

class PowerMeterComponent : public PollingComponent
{
 public:
  void setup() override;
  void update() override;
  void loop() override;
  void dump_config() override;

  void set_global_voltage_sensor(sensor::Sensor *sensor) { this->internal_channels[0].set_Vrms_sensor(sensor); }
  void set_Irms_sensor(int index, sensor::Sensor *sensor) { this->internal_channels[index].set_Irms_sensor(sensor); }
  void set_Papp_sensor(int index, sensor::Sensor *sensor) { this->internal_channels[index].set_Papp_sensor(sensor); }
  void set_Preal_sensor(int index, sensor::Sensor *sensor) { this->internal_channels[index].set_Preal_sensor(sensor); }
  void set_power_factor_sensor(int index, sensor::Sensor *sensor) { this->internal_channels[index].set_power_factor_sensor(sensor); }
  void set_phase_sensor(int index, sensor::Sensor *sensor) { this->internal_channels[index].set_phase_sensor(sensor); }
  void set_activeEnergy_sensor(int index, sensor::Sensor *sensor) { this->internal_channels[index].set_activeEnergy_sensor(sensor); }
  void set_reactiveEnergy_sensor(int index, sensor::Sensor *sensor) { this->internal_channels[index].set_reactiveEnergy_sensor(sensor); }
  void set_timestamp_sensor(int index, sensor::Sensor *sensor) { this->internal_channels[index].set_timestamp_sensor(sensor); }
  void set_state_sensor(int index, text_sensor::TextSensor *sensor) { this->internal_channels[index].set_state_sensor(sensor); }

  /*void set_voltage_calibration(number::Number *voltage_calibration) {
    this->internal_channels[0].calibration_ratio(voltage_calibration);
  }*/

 private:
  uint8_t result_mask = 0;
  sensor::Sensor *global_voltage_sensor = nullptr;

  PowerMeterChannel internal_channels[CHANNELS] = {
    {0, &channels[0]},
    {1, &channels[1]},
    {2, &channels[2]},
    {3, &channels[3]},
    {4, &channels[4]},
    {5, &channels[5]},
    {6, &channels[6]}
  };
};

}  // namespace empty_spi_sensor
}  // namespace esphome
