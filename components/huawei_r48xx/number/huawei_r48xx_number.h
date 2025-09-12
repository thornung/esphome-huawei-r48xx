#pragma once
#include "../huawei_r48xx.h"
#include "esphome/core/component.h"
#include "esphome/components/number/number.h"

namespace esphome {
namespace huawei_r48xx {

class HuaweiR48xxNumber : public number::Number, public Component {
 public:
  void set_parent(HuaweiR48xxComponent *parent, int8_t functionCode) {
    this->parent_ = parent;
    this->functionCode_ = functionCode;
  };

  void control(float value) override;

 protected:
  HuaweiR48xxComponent *parent_;
  int8_t functionCode_;
};

}  // namespace huawei_r48xx
}  // namespace esphome
