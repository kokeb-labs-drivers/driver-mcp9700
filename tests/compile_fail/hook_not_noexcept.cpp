/**
 * @file hook_not_noexcept.cpp
 * @brief Must NOT compile: a ReadVoltageMv hook that is not noexcept breaks the port contract, so
 * the port concept rejects it (handbook KB-CPP20-006).
 *
 * EXPECT: AdcPort
 */
#include <cstddef>
#include <cstdint>

#include "mcp9700.hpp"

class ThrowingPort : public mcp9700::AdcInterface<ThrowingPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool ReadVoltageMv(uint8_t channel, uint32_t& voltage_mv) {
    voltage_mv = 0U;
    return channel == 0U;
  }
  void DelayUs(uint32_t us) noexcept {
    (void)us;
  }
};

int main() {
  ThrowingPort port;
  mcp9700::MCP9700<ThrowingPort> sensor(port);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
