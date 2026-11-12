/**
 * @file main.cpp
 * @brief Builds the driver from the installed package only: headers, version header and .ipp.
 */
#include <cstdint>

#include "mcp9700.hpp"

namespace {

// An ADC that always reads 0 mV: enough to instantiate the whole driver template.
class ZeroPort : public mcp9700::AdcInterface<ZeroPort> {
public:
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool ReadVoltageMv(uint8_t channel, uint32_t& voltage_mv) noexcept {
    (void)channel;
    voltage_mv = 0;
    return true;
  }
  void DelayUs(uint32_t us) noexcept {
    (void)us;
  }
};

} // namespace

int main() {
  ZeroPort port;
  mcp9700::MCP9700<ZeroPort> sensor(port);
  // 0 mV is outside the sensor's range, so EnsureInitialized() must fail with DeviceIdMismatch.
  const auto r = sensor.EnsureInitialized();
  return (r.error == mcp9700::Error::DeviceIdMismatch && mcp9700::GetDriverVersion()[0] != '\0')
             ? 0
             : 1;
}
