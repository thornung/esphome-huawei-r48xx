#pragma once
#include "esphome/core/component.h"
#include "esphome/components/sensor/sensor.h"
#include "esphome/components/number/number.h"
#include "esphome/components/canbus/canbus.h"
namespace esphome {
namespace huawei_r48xx {
class HuaweiR48xxComponent : public PollingComponent {
 public:
  HuaweiR48xxComponent(canbus::Canbus *canbus);
  void setup() override;
  void update() override;
  void set_output_voltage(float value, bool offline = false);
  void set_max_output_current(float value, bool offline = false);
  void set_offline_values();
  void set_input_voltage_sensor(sensor::Sensor *input_voltage_sensor) { input_voltage_sensor_ = input_voltage_sensor; }
  void set_input_frequency_sensor(sensor::Sensor *input_frequency_sensor) {
    input_frequency_sensor_ = input_frequency_sensor;
  }
  void set_input_current_sensor(sensor::Sensor *input_current_sensor) { input_current_sensor_ = input_current_sensor; }
  void set_input_power_sensor(sensor::Sensor *input_power_sensor) { input_power_sensor_ = input_power_sensor; }
  void set_input_temp_sensor(sensor::Sensor *input_temp_sensor) { input_temp_sensor_ = input_temp_sensor; }
  void set_efficiency_sensor(sensor::Sensor *efficiency_sensor) { efficiency_sensor_ = efficiency_sensor; }
  void set_output_voltage_sensor(sensor::Sensor *output_voltage_sensor) {
    output_voltage_sensor_ = output_voltage_sensor;
  }
  void set_output_current_sensor(sensor::Sensor *output_current_sensor) {
    output_current_sensor_ = output_current_sensor;
  }
  void set_output_power_sensor(sensor::Sensor *output_power_sensor) { output_power_sensor_ = output_power_sensor; }
  void set_output_temp_sensor(sensor::Sensor *output_temp_sensor) { output_temp_sensor_ = output_temp_sensor; }
  void set_output_voltage_number(number::Number *output_voltage_number) {
    output_voltage_number_ = output_voltage_number;
  }
  void set_max_output_current_number(number::Number *max_output_current_number) {
    max_output_current_number_ = max_output_current_number;
  }
  // Kalibrierungsfaktor abrufen (für Diagnose)
  float get_current_scale_factor() const { return current_scale_factor_; }
 protected:
  canbus::Canbus *canbus;
  uint32_t lastUpdate_;
  // Automatische Stromkalibrierung
  float current_scale_factor_{10.08f};  // Startwert R4875G1, wird laufend kalibriert
  int32_t last_raw_sent_{0};
  sensor::Sensor *input_voltage_sensor_{nullptr};
  sensor::Sensor *input_frequency_sensor_{nullptr};
  sensor::Sensor *input_current_sensor_{nullptr};
  sensor::Sensor *input_power_sensor_{nullptr};
  sensor::Sensor *input_temp_sensor_{nullptr};
  sensor::Sensor *efficiency_sensor_{nullptr};
  sensor::Sensor *output_voltage_sensor_{nullptr};
  sensor::Sensor *output_current_sensor_{nullptr};
  sensor::Sensor *output_power_sensor_{nullptr};
  sensor::Sensor *output_temp_sensor_{nullptr};
  number::Number *output_voltage_number_{nullptr};
  number::Number *max_output_current_number_{nullptr};
  void on_frame(uint32_t can_id, bool rtr, std::vector<uint8_t> &data);
  void publish_sensor_state_(sensor::Sensor *sensor, float value);
  void publish_number_state_(number::Number *number, float value);
};
}  // namespace huawei_r48xx
}  // namespace esphome