/**
 * @file gpio_set_returns_void.cpp
 * @brief Must NOT compile: an optional hook with the wrong signature is rejected, not ignored.
 *
 * EXPECT: GpioSet hook must be
 */
#include "mcp9700.hpp"
#include "mock_mcp9700.hpp"

class VoidGpioPort : public mcp9700_test::MockPortBase<VoidGpioPort> {
public:
  void DelayUs(uint32_t us) noexcept {
    (void)us;
  }
  void GpioSet(mcp9700::CtrlPin pin, mcp9700::GpioSignal signal) noexcept {
    (void)pin;
    (void)signal;
  }
};

int main() {
  VoidGpioPort port;
  mcp9700::MCP9700<VoidGpioPort> sensor(port);
  return static_cast<int>(sensor.EnsureInitialized().error);
}
