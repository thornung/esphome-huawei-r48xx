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

  void set_input_voltage_sensor(sensor::Sensor *s)      { input_voltage_sensor_ = s; }
  void set_input_frequency_sensor(sensor::Sensor *s)    { input_frequency_sensor_ = s; }
  void set_input_current_sensor(sensor::Sensor *s)      { input_current_sensor_ = s; }
  void set_input_power_sensor(sensor::Sensor *s)        { input_power_sensor_ = s; }
  void set_input_temp_sensor(sensor::Sensor *s)         { input_temp_sensor_ = s; }
  void set_efficiency_sensor(sensor::Sensor *s)         { efficiency_sensor_ = s; }
  void set_output_voltage_sensor(sensor::Sensor *s)     { output_voltage_sensor_ = s; }
  void set_output_current_sensor(sensor::Sensor *s)     { output_current_sensor_ = s; }
  void set_output_power_sensor(sensor::Sensor *s)       { output_power_sensor_ = s; }
  void set_output_temp_sensor(sensor::Sensor *s)        { output_temp_sensor_ = s; }
  void set_output_voltage_number(number::Number *n)     { output_voltage_number_ = n; }
  void set_max_output_current_number(number::Number *n) { max_output_current_number_ = n; }

  float get_current_scale_factor() const { return current_scale_factor_; }
  void set_measurement_scale(float s) { measurement_scale_ = s; }
  void set_measurement_scale_auto(bool a) { auto_scale_ = a; }

 protected:
  canbus::Canbus *canbus;
  uint32_t lastUpdate_{0};

  // Automatische Stromkalibrierung
  float   current_scale_factor_{10.08f};  // Startwert R4875G1, wird laufend kalibriert
  int32_t last_raw_sent_{0};

  // Korrektur fuer gemeldeten Strom/Leistung (R4875G1 und R4850: 1.0)
  float measurement_scale_{1.0f};
  bool auto_scale_{false};  // true: Faktor aus erkanntem Geraetetyp

  // Geraeteerkennung (CAN-Info-Abfrage 0x108150FE)
  bool identified_{false};
  bool nominal_known_{false};
  float nominal_current_{0.0f};
  char barcode_[13]{0};
  uint32_t ident_start_ms_{0};
  uint32_t last_ident_req_ms_{0};
  void request_identity_();
  void on_identity_frame_(const std::vector<uint8_t> &data, bool last);
  void apply_model_();
  void log_identity_();
  char ident_log_[3][128]{};  // Ergebniszeilen, werden nach 60 s nochmal ausgegeben
  uint8_t ident_log_n_{0};
  bool ident_relogged_{false};

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
