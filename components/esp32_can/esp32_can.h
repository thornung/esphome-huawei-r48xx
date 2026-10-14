#pragma once

#ifdef USE_ESP32

#include "esphome/components/canbus/canbus.h"
#include "esphome/core/component.h"

namespace esphome {
namespace esp32_can {

class ESP32Can : public canbus::Canbus {
 public:
  void set_rx(int rx) { rx_ = rx; }
  void set_tx(int tx) { tx_ = tx; }
  ESP32Can(){};

  // Ueberwacht den TWAI-Controller (Bus-Off, gestoppt, RX-Stillstand)
  // und liest danach wie gewohnt die Frames.
  void loop() override;

 protected:
  bool setup_internal() override;
  canbus::Error send_message(struct canbus::CanFrame *frame) override;
  canbus::Error read_message(struct canbus::CanFrame *frame) override;

  bool install_();                    // Treiber installieren + starten (ohne mark_failed)
  void check_bus_(uint32_t now);      // Zustand pruefen, ggf. Recovery/Neustart
  void log_status_(const char *why);  // Status-Zaehler ins Log

  int rx_{-1};
  int tx_{-1};

  uint32_t last_check_ms_{0};
  uint32_t last_rx_ms_{0};
  uint32_t last_reinit_ms_{0};
  uint32_t bus_off_count_{0};
  uint32_t reinit_count_{0};
};

}  // namespace esp32_can
}  // namespace esphome

#endif
