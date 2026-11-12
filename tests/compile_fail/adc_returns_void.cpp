/**
 * @file adc_returns_void.cpp
 * @brief Must NOT compile: a ReadVoltageMv hook that returns void cannot report a failed
 * conversion, so the port concept rejects it (handbook KB-CPP20-006).
 *
 * EXPECT: AdcPort
 */
#include <cstdint>

#include "mcp9700.hpp"

class VoidPort : public mcp9700::AdcInterface<VoidPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  void ReadVoltageMv(uint8_t channel, uint32_t& voltage_mv) noexcept {
    voltage_mv = channel;
  }
  void DelayUs(uint32_t us) noexcept {
    (void)us;
  }
};

int main() {
  VoidPort port;
  mcp9700::MCP9700<VoidPort> sensor(port);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
