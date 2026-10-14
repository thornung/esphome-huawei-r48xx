#ifdef USE_ESP32
#include "esp32_can.h"
#include "esphome/core/log.h"
#include "esphome/core/hal.h"

#include <driver/twai.h>

// WORKAROUND, because CAN_IO_UNUSED is just defined as (-1) in this version
// of the framework which does not work with -fpermissive
#undef CAN_IO_UNUSED
#define CAN_IO_UNUSED ((gpio_num_t) -1)

namespace esphome {
namespace esp32_can {

static const char *const TAG = "esp32_can";

// Kein Frame empfangen fuer so lange (ms) -> Treiber neu installieren
static const uint32_t RX_STALL_MS = 30000;
// Frühestens alle ... ms erneut neu installieren (R4875 kann auch wirklich aus sein)
static const uint32_t REINIT_MIN_INTERVAL_MS = 60000;

static bool get_bitrate(canbus::CanSpeed bitrate, twai_timing_config_t *t_config) {
  switch (bitrate) {
    case canbus::CAN_50KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_50KBITS();
      return true;
    case canbus::CAN_100KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_100KBITS();
      return true;
    case canbus::CAN_125KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_125KBITS();
      return true;
    case canbus::CAN_250KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_250KBITS();
      return true;
    case canbus::CAN_500KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_500KBITS();
      return true;
    case canbus::CAN_1000KBPS:
      *t_config = (twai_timing_config_t) TWAI_TIMING_CONFIG_1MBITS();
      return true;
    default:
      return false;
  }
}

bool ESP32Can::install_() {
  twai_general_config_t g_config =
      TWAI_GENERAL_CONFIG_DEFAULT((gpio_num_t) this->tx_, (gpio_num_t) this->rx_, TWAI_MODE_NORMAL);
  g_config.rx_queue_len = 12;
  twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();
  twai_timing_config_t t_config;

  if (!get_bitrate(this->bit_rate_, &t_config)) {
    return false;  // invalid bit rate
  }
  if (twai_driver_install(&g_config, &t_config, &f_config) != ESP_OK) {
    return false;
  }
  if (twai_start() != ESP_OK) {
    twai_driver_uninstall();
    return false;
  }
  this->last_rx_ms_ = millis();
  return true;
}

bool ESP32Can::setup_internal() {
  if (!this->install_()) {
    this->mark_failed();
    return false;
  }
  return true;
}

void ESP32Can::log_status_(const char *why) {
  twai_status_info_t st;
  if (twai_get_status_info(&st) != ESP_OK) {
    ESP_LOGW(TAG, "%s: Status nicht lesbar (Treiber nicht installiert?)", why);
    return;
  }
  const char *state = "?";
  switch (st.state) {
    case TWAI_STATE_STOPPED: state = "STOPPED"; break;
    case TWAI_STATE_RUNNING: state = "RUNNING"; break;
    case TWAI_STATE_BUS_OFF: state = "BUS_OFF"; break;
    case TWAI_STATE_RECOVERING: state = "RECOVERING"; break;
  }
  ESP_LOGW(TAG,
           "%s: state=%s tx_err=%u rx_err=%u msgs_to_tx=%u msgs_to_rx=%u tx_failed=%u rx_missed=%u "
           "arb_lost=%u bus_err=%u (bus_off#%u, reinit#%u, letzter RX vor %us)",
           why, state, (unsigned) st.tx_error_counter, (unsigned) st.rx_error_counter, (unsigned) st.msgs_to_tx,
           (unsigned) st.msgs_to_rx, (unsigned) st.tx_failed_count, (unsigned) st.rx_missed_count,
           (unsigned) st.arb_lost_count, (unsigned) st.bus_error_count, (unsigned) this->bus_off_count_,
           (unsigned) this->reinit_count_, (unsigned) ((millis() - this->last_rx_ms_) / 1000));
}

void ESP32Can::check_bus_(uint32_t now) {
  twai_status_info_t st;
  if (twai_get_status_info(&st) != ESP_OK) {
    return;  // Treiber nicht installiert
  }

  if (st.state == TWAI_STATE_BUS_OFF) {
    // Controller hat sich wegen zu vieler Fehler vom Bus getrennt -> Recovery anstossen
    this->bus_off_count_++;
    this->log_status_("TWAI BUS-OFF, starte Recovery");
    twai_initiate_recovery();
    return;
  }

  if (st.state == TWAI_STATE_STOPPED) {
    // Nach erfolgreicher Recovery steht der Controller auf STOPPED -> wieder starten
    this->log_status_("TWAI gestoppt, starte Treiber");
    twai_clear_transmit_queue();
    twai_clear_receive_queue();
    if (twai_start() == ESP_OK) {
      this->last_rx_ms_ = now;
    }
    return;
  }

  // Laeuft zwar, aber seit langem kein Frame mehr empfangen -> Treiber komplett neu installieren
  if (st.state == TWAI_STATE_RUNNING && (now - this->last_rx_ms_) > RX_STALL_MS &&
      (now - this->last_reinit_ms_) > REINIT_MIN_INTERVAL_MS) {
    this->last_reinit_ms_ = now;
    this->reinit_count_++;
    this->log_status_("Kein CAN-Frame empfangen, installiere TWAI-Treiber neu");
    twai_stop();
    twai_driver_uninstall();
    if (!this->install_()) {
      ESP_LOGE(TAG, "TWAI Neuinstallation fehlgeschlagen - naechster Versuch in %us",
               (unsigned) (REINIT_MIN_INTERVAL_MS / 1000));
    } else {
      ESP_LOGW(TAG, "TWAI Treiber neu installiert");
    }
  }
}

void ESP32Can::loop() {
  const uint32_t now = millis();
  if (now - this->last_check_ms_ >= 500) {
    this->last_check_ms_ = now;
    this->check_bus_(now);
  }
  canbus::Canbus::loop();
}

canbus::Error ESP32Can::send_message(struct canbus::CanFrame *frame) {
  if (frame->can_data_length_code > canbus::CAN_MAX_DATA_LENGTH) {
    return canbus::ERROR_FAILTX;
  }

  uint32_t flags = TWAI_MSG_FLAG_NONE;
  if (frame->use_extended_id) {
    flags |= TWAI_MSG_FLAG_EXTD;
  }
  if (frame->remote_transmission_request) {
    flags |= TWAI_MSG_FLAG_RTR;
  }

  twai_message_t message = {
      .flags = flags,
      .identifier = frame->can_id,
      .data_length_code = frame->can_data_length_code,
  };
  if (!frame->remote_transmission_request) {
    memcpy(message.data, frame->data, frame->can_data_length_code);
  }

  if (twai_transmit(&message, pdMS_TO_TICKS(10)) == ESP_OK) {
    return canbus::ERROR_OK;
  } else {
    return canbus::ERROR_ALLTXBUSY;
  }
}

canbus::Error ESP32Can::read_message(struct canbus::CanFrame *frame) {
  twai_message_t message;

  if (twai_receive(&message, 0) != ESP_OK) {
    return canbus::ERROR_NOMSG;
  }
  this->last_rx_ms_ = millis();

  frame->can_id = message.identifier;
  frame->use_extended_id = message.flags & TWAI_MSG_FLAG_EXTD;
  frame->remote_transmission_request = message.flags & TWAI_MSG_FLAG_RTR;
  frame->can_data_length_code = message.data_length_code;

  if (!frame->remote_transmission_request) {
    size_t dlc =
        message.data_length_code < canbus::CAN_MAX_DATA_LENGTH ? message.data_length_code : canbus::CAN_MAX_DATA_LENGTH;
    memcpy(frame->data, message.data, dlc);
  }

  return canbus::ERROR_OK;
}

}  // namespace esp32_can
}  // namespace esphome

#endif
