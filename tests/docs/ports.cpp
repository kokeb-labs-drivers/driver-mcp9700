/**
 * @file ports.cpp
 * @brief The port with every optional hook, as shown on the Porting page. Compiled, never run.
 */
#include <cstdint>

#include "mcp9700.hpp"

bool platform_adc_init() noexcept;
bool platform_adc_read_mv(uint8_t channel, uint32_t& mv) noexcept;
void platform_delay_us(uint32_t us) noexcept;
bool platform_gpio_write(int pin, bool high) noexcept;
void platform_log(const char* line) noexcept;

// --8<-- [start:full-port]
class FullPort : public mcp9700::AdcInterface<FullPort> {
public:
  bool EnsureInitialized() noexcept {
    return platform_adc_init();
  }
  bool ReadVoltageMv(uint8_t channel, uint32_t& voltage_mv) noexcept {
    return platform_adc_read_mv(channel, voltage_mv);
  }
  void DelayUs(uint32_t us) noexcept {
    platform_delay_us(us);
  }

  // Optional: the sensor's supply from an MCU pin (DS20001942L Section 4.2). ACTIVE means
  // "powered". The pin idles INACTIVE until the driver enables it.
  bool GpioSet(mcp9700::CtrlPin pin, mcp9700::GpioSignal signal) noexcept {
    if (pin != mcp9700::CtrlPin::Enable) {
      return false;
    }
    return platform_gpio_write(kSupplyGpio, signal == mcp9700::GpioSignal::ACTIVE);
  }

  // Optional: one line per failure. The driver never formats; the port does.
  void Log(mcp9700::LogLevel level, const char* message) noexcept {
    (void)level;
    platform_log(message); // plain text: never use it as a format string
  }

private:
  static constexpr int kSupplyGpio = 7;
};
// --8<-- [end:full-port]
