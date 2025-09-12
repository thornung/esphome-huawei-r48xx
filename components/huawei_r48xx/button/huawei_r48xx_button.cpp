#include "huawei_r48xx_button.h"
#include "esphome/core/log.h"

namespace esphome {
namespace huawei_r48xx {

void HuaweiR48xxButton::press_action() { this->parent_->set_offline_values(); }

}  // namespace huawei_r48xx
}  // namespace esphome
