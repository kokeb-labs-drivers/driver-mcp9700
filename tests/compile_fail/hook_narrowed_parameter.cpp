/**
 * @file hook_narrowed_parameter.cpp
 * @brief Must NOT compile: a ReadVoltageMv hook whose channel is a wider type accepts what the
 * contract does not, so the port concept rejects it (handbook KB-CPP20-006).
 *
 * EXPECT: AdcPort
 */
#include <cstddef>
#include <cstdint>

#include "mcp9700.hpp"

class NarrowPort : public mcp9700::AdcInterface<NarrowPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool ReadVoltageMv(uint16_t channel, uint32_t& voltage_mv) noexcept {
    voltage_mv = 0U;
    return channel == 0U;
  }
  void DelayUs(uint32_t us) noexcept {
    (void)us;
  }
};

int main() {
  NarrowPort port;
  mcp9700::MCP9700<NarrowPort> sensor(port);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
