#include "huawei_r48xx.h"
#include "esphome/core/application.h"
#include "esphome/core/base_automation.h"
#include "esphome/core/automation.h"
#include "esphome/core/component.h"
#include "esphome/core/log.h"
#include <cmath>
#include <cstring>

namespace esphome {
namespace huawei_r48xx {

static const char *const TAG = "huawei_r48xx";

static const uint32_t CAN_ID_REQUEST = 0x108040FE;
static const uint32_t CAN_ID_DATA = 0x1081407F;
static const uint32_t CAN_ID_SET = 0x108180FE;
static const uint32_t CAN_ID_INFO_REQUEST = 0x108150FE;  // Info-Abfrage (Nennstrom, Barcode, Versionen)
static const uint32_t CAN_ID_INFO_DATA = 0x1081507F;     // Antwort-Frames
static const uint32_t CAN_ID_INFO_LAST = 0x1081507E;     // letzter Antwort-Frame

static const uint32_t IDENT_RETRY_MS = 2000;
static const uint32_t IDENT_TIMEOUT_MS = 30000;

// Typ-Tabelle: Nennstrom aus Info-Register 0x0001, Faktor fuer gemeldeten Strom/Leistung.
// Der Messfaktor ist relativ zur R4850-Vorlage (52 A => 1.0): 52 / Nennstrom.
struct R48xxModel {
  const char *name;
  float nominal_a;
  float meas_scale;
};
static const R48xxModel R48XX_MODELS[] = {
    {"R4850G2", 52.0f, 1.0f},
    {"R4875G1", 101.5f, 0.512f},  // Nennstrom aus dem empirischen Schreibfaktor 10.08 abgeleitet (1024/10.08)
};
static const float MODEL_MATCH_TOLERANCE_A = 4.0f;

static const uint8_t R48xx_DATA_INPUT_POWER = 0x70;
static const uint8_t R48xx_DATA_INPUT_FREQ = 0x71;
static const uint8_t R48xx_DATA_INPUT_CURRENT = 0x72;
static const uint8_t R48xx_DATA_OUTPUT_POWER = 0x73;
static const uint8_t R48xx_DATA_EFFICIENCY = 0x74;
static const uint8_t R48xx_DATA_OUTPUT_VOLTAGE = 0x75;
static const uint8_t R48xx_DATA_OUTPUT_CURRENT_MAX = 0x76;
static const uint8_t R48xx_DATA_INPUT_VOLTAGE = 0x78;
static const uint8_t R48xx_DATA_OUTPUT_TEMPERATURE = 0x7F;
static const uint8_t R48xx_DATA_INPUT_TEMPERATURE = 0x80;
static const uint8_t R48xx_DATA_OUTPUT_CURRENT = 0x81;
static const uint8_t R48xx_DATA_OUTPUT_CURRENT1 = 0x82;

HuaweiR48xxComponent::HuaweiR48xxComponent(canbus::Canbus *canbus) {
  this->canbus = canbus;
  this->current_scale_factor_ = 10.08f;  // Startwert R4875G1, wird automatisch kalibriert
  this->last_raw_sent_ = 0;
}

void HuaweiR48xxComponent::setup() {
#if ESPHOME_VERSION_CODE >= VERSION_CODE(2026, 2, 0)
  // ESPHome >= 2026.2: CanbusTrigger ist kein Trigger<Ts...> mehr
  // → direkt add_callback() auf Canbus verwenden
  this->canbus->add_callback([this](uint32_t can_id, bool extended_id, bool rtr, const std::vector<uint8_t> &data) {
    this->on_frame(can_id, rtr, const_cast<std::vector<uint8_t> &>(data));
  });
#else
  // ESPHome < 2026.2: Automation über CanbusTrigger
  Automation<std::vector<uint8_t>, uint32_t, bool> *automation;
  LambdaAction<std::vector<uint8_t>, uint32_t, bool> *lambdaaction;
  canbus::CanbusTrigger *canbus_canbustrigger;
  canbus_canbustrigger = new canbus::CanbusTrigger(this->canbus, 0, 0, true);
  canbus_canbustrigger->set_component_source("canbus");
  App.register_component(canbus_canbustrigger);
  automation = new Automation<std::vector<uint8_t>, uint32_t, bool>(canbus_canbustrigger);
  auto cb = [=](std::vector<uint8_t> x, uint32_t can_id, bool remote_transmission_request) -> void {
    this->on_frame(can_id, remote_transmission_request, x);
  };
  lambdaaction = new LambdaAction<std::vector<uint8_t>, uint32_t, bool>(cb);
  automation->add_actions({lambdaaction});
#endif
}

void HuaweiR48xxComponent::request_identity_() {
  const uint32_t now = millis();
  if (this->ident_start_ms_ == 0)
    this->ident_start_ms_ = now;
  if (now - this->ident_start_ms_ > IDENT_TIMEOUT_MS) {
    this->identified_ = true;  // aufgeben, mit dem konfigurierten Faktor weiterarbeiten
    if (this->auto_scale_) {
      ESP_LOGE(TAG, "Geraet nicht erkannt (keine Antwort auf Info-Abfrage) - verwende measurement_scale=%.3f",
               this->measurement_scale_);
    } else {
      ESP_LOGW(TAG, "Geraet nicht erkannt (keine Antwort auf Info-Abfrage)");
    }
    return;
  }
  if (now - this->last_ident_req_ms_ >= IDENT_RETRY_MS) {
    this->last_ident_req_ms_ = now;
    this->nominal_known_ = false;
    memset(this->barcode_, 0, sizeof(this->barcode_));
    std::vector<uint8_t> req = {0, 0, 0, 0, 0, 0, 0, 0};
    this->canbus->send_data(CAN_ID_INFO_REQUEST, true, req);
  }
}

void HuaweiR48xxComponent::on_identity_frame_(const std::vector<uint8_t> &data, bool last) {
  if (this->identified_ || data.size() < 8)
    return;
  const uint16_t reg = (data[0] << 8) | data[1];
  switch (reg) {
    case 0x0001: {
      // Modul-Kenndaten: Nennstrom = ((Byte4<<8 | Byte5) & 0x3FF) / 2  [A]
      const uint16_t raw = ((data[4] << 8) | data[5]) & 0x3FF;
      this->nominal_current_ = raw / 2.0f;
      this->nominal_known_ = true;
      break;
    }
    case 0x0003:
      memcpy(this->barcode_, &data[2], 6);  // Barcode Teil 1 (ASCII)
      break;
    case 0x0004:
      memcpy(this->barcode_ + 6, &data[2], 6);  // Barcode Teil 2 (ASCII)
      break;
    default:
      break;
  }
  if (last) {
    if (this->nominal_known_) {
      this->apply_model_();
    }
    // sonst: naechster Versuch nach IDENT_RETRY_MS
  }
}

void HuaweiR48xxComponent::apply_model_() {
  const R48xxModel *best = nullptr;
  float best_d = 1e9f;
  for (const auto &m : R48XX_MODELS) {
    const float d = fabsf(m.nominal_a - this->nominal_current_);
    if (d < best_d) {
      best_d = d;
      best = &m;
    }
  }
  const bool known = (best != nullptr && best_d <= MODEL_MATCH_TOLERANCE_A);
  const float expected_write = 1024.0f / this->nominal_current_;
  ESP_LOGI(TAG, "Geraet erkannt: %s, Nennstrom=%.1fA, Barcode=%s", known ? best->name : "unbekannt",
           this->nominal_current_, this->barcode_);
  ESP_LOGI(TAG, "  Schreibfaktor Stromlimit: erwartet %.3f (=1024/Nennstrom), aktuell %.3f", expected_write,
           this->current_scale_factor_);
  if (known) {
    if (this->auto_scale_) {
      this->measurement_scale_ = best->meas_scale;
      ESP_LOGI(TAG, "  measurement_scale=auto -> %.3f", this->measurement_scale_);
    } else if (fabsf(best->meas_scale - this->measurement_scale_) > 0.02f) {
      ESP_LOGW(TAG, "  konfigurierter measurement_scale=%.3f weicht vom Typ-Wert %.3f ab", this->measurement_scale_,
               best->meas_scale);
    } else {
      ESP_LOGI(TAG, "  konfigurierter measurement_scale=%.3f passt zum Typ", this->measurement_scale_);
    }
  } else {
    ESP_LOGW(TAG, "  Typ nicht in der Tabelle - measurement_scale bleibt %.3f (Tabelle R48XX_MODELS ergaenzen)",
             this->measurement_scale_);
  }
  this->identified_ = true;
}

void HuaweiR48xxComponent::update() {
  if (!this->identified_)
    this->request_identity_();
  ESP_LOGD(TAG, "Sending request message");
  std::vector<uint8_t> data = {0, 0, 0, 0, 0, 0, 0, 0};
  this->canbus->send_data(CAN_ID_REQUEST, true, data);

  // no new value for 5* intervall -> set sensors to NAN)
  if (millis() - lastUpdate_ > this->update_interval_ * 5) {
    this->publish_sensor_state_(this->input_power_sensor_, NAN);
    this->publish_sensor_state_(this->input_voltage_sensor_, NAN);
    this->publish_sensor_state_(this->input_current_sensor_, NAN);
    this->publish_sensor_state_(this->input_temp_sensor_, NAN);
    this->publish_sensor_state_(this->input_frequency_sensor_, NAN);
    this->publish_sensor_state_(this->output_power_sensor_, NAN);
    this->publish_sensor_state_(this->output_current_sensor_, NAN);
    this->publish_sensor_state_(this->output_voltage_sensor_, NAN);
    this->publish_sensor_state_(this->output_temp_sensor_, NAN);
    this->publish_sensor_state_(this->efficiency_sensor_, NAN);
    this->publish_number_state_(this->max_output_current_number_, NAN);
  }
}

void HuaweiR48xxComponent::set_output_voltage(float value, bool offline) {
  uint8_t functionCode = 0x0;
  if (offline)
    functionCode += 1;
  int32_t raw = 1024.0 * value;
  std::vector<uint8_t> data = {
      0x1, functionCode, 0x0, 0x0, (uint8_t) (raw >> 24), (uint8_t) (raw >> 16), (uint8_t) (raw >> 8), (uint8_t) raw};
  this->canbus->send_data(CAN_ID_SET, true, data);
}

void HuaweiR48xxComponent::set_max_output_current(float value, bool offline) {
  uint8_t functionCode = 0x3;
  if (offline)
    functionCode += 1;
  int32_t raw = (int32_t)(this->current_scale_factor_ * value);
  this->last_raw_sent_ = raw;
  std::vector<uint8_t> data = {
      0x1, functionCode, 0x0, 0x0, (uint8_t) (raw >> 24), (uint8_t) (raw >> 16), (uint8_t) (raw >> 8), (uint8_t) raw};
  this->canbus->send_data(CAN_ID_SET, true, data);
}

void HuaweiR48xxComponent::set_offline_values() {
  if (output_voltage_number_) {
    set_output_voltage(output_voltage_number_->state, true);
  };
  if (max_output_current_number_) {
    set_max_output_current(max_output_current_number_->state, true);
  }
}

void HuaweiR48xxComponent::on_frame(uint32_t can_id, bool rtr, std::vector<uint8_t> &data) {
  if (can_id == CAN_ID_INFO_DATA || can_id == CAN_ID_INFO_LAST) {
    this->on_identity_frame_(data, can_id == CAN_ID_INFO_LAST);
    return;
  }
  if (can_id == CAN_ID_DATA) {
    uint32_t value = (data[4] << 24) + (data[5] << 16) + (data[6] << 8) + data[7];
    float conv_value = 0;
    switch (data[1]) {
      case R48xx_DATA_INPUT_POWER:
        if (this->auto_scale_ && !this->identified_)
          break;  // Faktor noch unbekannt
        conv_value = value / 1024.0 * this->measurement_scale_;
        this->publish_sensor_state_(this->input_power_sensor_, conv_value);
        ESP_LOGV(TAG, "Input power: %f", conv_value);
        break;

      case R48xx_DATA_INPUT_FREQ:
        conv_value = value / 1024.0;
        this->publish_sensor_state_(this->input_frequency_sensor_, conv_value);
        ESP_LOGV(TAG, "Input frequency: %f", conv_value);
        break;

      case R48xx_DATA_INPUT_CURRENT:
        if (this->auto_scale_ && !this->identified_)
          break;  // Faktor noch unbekannt
        conv_value = value / 1024.0 * this->measurement_scale_;
        this->publish_sensor_state_(this->input_current_sensor_, conv_value);
        ESP_LOGV(TAG, "Input current: %f", conv_value);
        break;

      case R48xx_DATA_OUTPUT_POWER:
        if (this->auto_scale_ && !this->identified_)
          break;  // Faktor noch unbekannt
        conv_value = value / 1024.0 * this->measurement_scale_;
        this->publish_sensor_state_(this->output_power_sensor_, conv_value);
        ESP_LOGV(TAG, "Output power: %f", conv_value);
        break;

      case R48xx_DATA_EFFICIENCY:
        conv_value = value / 1024.0 * 100;
        this->publish_sensor_state_(this->efficiency_sensor_, conv_value);
        ESP_LOGV(TAG, "Efficiency: %f", conv_value);
        break;

      case R48xx_DATA_OUTPUT_VOLTAGE:
        conv_value = value / 1024.0;
        this->publish_sensor_state_(this->output_voltage_sensor_, conv_value);
        ESP_LOGV(TAG, "Output voltage: %f", conv_value);
        break;

      case R48xx_DATA_OUTPUT_CURRENT_MAX:
        // Gerät meldet zurück welchen Strom es als Maximum hat (in geräteeigener Skala)
        // Wir nutzen das zur automatischen Kalibrierung des Schreibfaktors:
        // Wenn wir raw=X gesendet haben und das Gerät Y Ampere meldet → Faktor = X / Y
        if (this->last_raw_sent_ > 10 && value > 0) {
          float i_rueck = (float)value / 19.6923f;  // Lesefaktor (unverändert)
          if (i_rueck > 1.0f) {
            float faktor_neu = (float)this->last_raw_sent_ / i_rueck;
            // Sanfter Mittelwert: Faktor nicht zu schnell ändern
            this->current_scale_factor_ = 0.9f * this->current_scale_factor_ + 0.1f * faktor_neu;
            ESP_LOGD(TAG, "Kalibrierung: raw=%d, i=%.2fA, faktor=%.3f",
                     this->last_raw_sent_, i_rueck, this->current_scale_factor_);
          }
        }
        conv_value = (float)value / 19.6923f;
        this->publish_number_state_(this->max_output_current_number_, conv_value);
        ESP_LOGV(TAG, "Max Output current: %f", conv_value);
        break;

      case R48xx_DATA_INPUT_VOLTAGE:
        conv_value = value / 1024.0;
        this->publish_sensor_state_(this->input_voltage_sensor_, conv_value);
        ESP_LOGV(TAG, "Input voltage: %f", conv_value);
        break;

      case R48xx_DATA_OUTPUT_TEMPERATURE:
        conv_value = value / 1024.0;
        this->publish_sensor_state_(this->output_temp_sensor_, conv_value);
        ESP_LOGV(TAG, "Output temperature: %f", conv_value);
        break;

      case R48xx_DATA_INPUT_TEMPERATURE:
        conv_value = value / 1024.0;
        this->publish_sensor_state_(this->input_temp_sensor_, conv_value);
        ESP_LOGV(TAG, "Input temperature: %f", conv_value);
        break;

      case R48xx_DATA_OUTPUT_CURRENT1:
        break;

      case R48xx_DATA_OUTPUT_CURRENT:
        if (this->auto_scale_ && !this->identified_)
          break;  // Faktor noch unbekannt
        conv_value = value / 1024.0 * this->measurement_scale_;
        this->publish_sensor_state_(this->output_current_sensor_, conv_value);
        ESP_LOGV(TAG, "Output current: %f", conv_value);
        this->lastUpdate_ = millis();
        break;

      default:
        break;
    }
  }
}

void HuaweiR48xxComponent::publish_sensor_state_(sensor::Sensor *sensor, float value) {
  if (sensor) {
    sensor->publish_state(value);
  }
}

void HuaweiR48xxComponent::publish_number_state_(number::Number *number, float value) {
  if (number) {
    number->publish_state(value);
  }
}

}  // namespace huawei_r48xx
}  // namespace esphome
