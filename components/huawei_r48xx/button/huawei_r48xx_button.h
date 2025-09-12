#pragma once
#include "../huawei_r48xx.h"
#include "esphome/core/component.h"
#include "esphome/components/button/button.h"

namespace esphome {
namespace huawei_r48xx {

class HuaweiR48xxButton : public button::Button, public Component {
 public:
  void set_parent(HuaweiR48xxComponent *parent) { this->parent_ = parent; };

 protected:
  HuaweiR48xxComponent *parent_;

  void press_action() override;
};

}  // namespace huawei_r48xx
}  // namespace esphome
