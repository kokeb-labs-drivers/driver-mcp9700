/**
 * @file my_port.hpp
 * @brief The ADC port shown on the Quickstart and Porting pages, compiled by the host build.
 */
#ifndef MCP9700_MY_PORT_HPP
#define MCP9700_MY_PORT_HPP

#include <cstdint>

#include "mcp9700_adc_interface.hpp"

// Your SDK's calls. Declared only: the docs build checks that the port compiles.
bool platform_adc_init() noexcept;
bool platform_adc_read_mv(uint8_t channel, uint32_t& mv) noexcept; // one calibrated conversion
void platform_delay_us(uint32_t us) noexcept;

// --8<-- [start:port]
class MyPort : public mcp9700::AdcInterface<MyPort> {
public:
  // Bring the ADC up once; later calls return true at once.
  bool EnsureInitialized() noexcept {
    if (!ready_) {
      ready_ = platform_adc_init();
    }
    return ready_;
  }

  // One conversion of `channel`, calibrated to millivolts.
  bool ReadVoltageMv(uint8_t channel, uint32_t& voltage_mv) noexcept {
    return platform_adc_read_mv(channel, voltage_mv);
  }

  // Wait at least `us` microseconds: power-up and sample spacing depend on it.
  void DelayUs(uint32_t us) noexcept {
    platform_delay_us(us);
  }

private:
  bool ready_ = false;
};
// --8<-- [end:port]

#endif // MCP9700_MY_PORT_HPP
