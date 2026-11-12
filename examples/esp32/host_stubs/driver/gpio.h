#ifndef HOST_STUB_DRIVER_GPIO_H
#define HOST_STUB_DRIVER_GPIO_H

#include <cstdint>

using esp_err_t = int;
constexpr esp_err_t ESP_OK = 0;
using gpio_num_t = int;
enum gpio_mode_t { GPIO_MODE_OUTPUT = 1 };

struct gpio_config_t {
  uint64_t pin_bit_mask;
  gpio_mode_t mode;
};

/// The last level written by `gpio_set_level()`, for the host check.
inline uint32_t g_stub_gpio_level = 0U;

inline esp_err_t gpio_set_level(gpio_num_t /*pin*/, uint32_t level) {
  g_stub_gpio_level = level;
  return ESP_OK;
}
inline esp_err_t gpio_config(const gpio_config_t* /*config*/) {
  return ESP_OK;
}

#endif // HOST_STUB_DRIVER_GPIO_H
