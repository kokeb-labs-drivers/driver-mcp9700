/**
 * @file main.cpp
 * @brief The MCP970x driver on your PC: start a simulated sensor, read it at three temperatures,
 * calibrate a part that reads 3 C high, see a pulled-out sensor, put the sensor to sleep and wake
 * it.
 *
 * No hardware and no SDK: the host port (host_mcp9700_port.hpp) connects the real driver to the
 * sensor and ADC model the host tests use. The same calls run on an MCU with that MCU's port
 * (examples/esp32/ for ESP-IDF). Temperatures print from integer milli-degrees; the floating-point
 * helper ToCelsius() is shown once. Exit code 0 when everything went as described.
 */
#include <cstdint>
#include <cstdio>

#include "host_mcp9700_port.hpp"
#include "mcp9700.hpp"

namespace {

/// Print `milli_c` as `[-]W.mmm C` with integer maths only.
void PrintTemperature(const char* label, int32_t milli_c) {
  const int32_t magnitude = (milli_c < 0) ? -milli_c : milli_c;
  (void)std::printf("%s %s%ld.%03ld C\n", label, (milli_c < 0) ? "-" : "",
                    static_cast<long>(magnitude / 1000), static_cast<long>(magnitude % 1000));
}

} // namespace

int main() {
  HostMcp9700Port port;
  mcp9700::MCP9700<HostMcp9700Port> sensor(port); // an MCP9700: no I/O yet

  (void)std::printf("MCP970x driver %s on the host\n", mcp9700::GetDriverVersion());
  if (const auto r = sensor.EnsureInitialized(); !r) {
    (void)std::printf("EnsureInitialized failed: %s\n", mcp9700::ToString(r.error));
    return 1;
  }
  (void)std::printf("one ADC step is %lu mC\n",
                    static_cast<unsigned long>(sensor.ResolutionMilliC()));

  for (const int32_t celsius : {25, 85, -10}) {
    port.state.temperature_mc = celsius * 1000;
    const auto m = sensor.ReadMeasurement();
    if (!m) {
      (void)std::printf("read failed: %s\n", mcp9700::ToString(m.error));
      return 1;
    }
    (void)std::printf("true %d C: VOUT %lu mV, spread %lu mV, ", celsius,
                      static_cast<unsigned long>(m.value.output_mv),
                      static_cast<unsigned long>(m.value.spread_mv));
    PrintTemperature("reads", m.value.temperature_mc);
  }
  const auto last = sensor.ReadTemperatureMilliC();
  (void)std::printf("as float: %.1f C\n", static_cast<double>(mcp9700::ToCelsius(last.value)));

  // One-point calibration (Section 4.1): a part that reads 3 C high at +25 C.
  port.state.temperature_mc = 25'000;
  port.state.accuracy_offset_mc = 3000;
  const auto raw = sensor.ReadTemperatureMilliC();
  PrintTemperature("part reads", raw.value);
  mcp9700::Config calibrated{};
  calibrated.offset_cal_mc = 25'000 - raw.value; // reference minus reading
  mcp9700::MCP9700<HostMcp9700Port> fixed(port, calibrated);
  if (!fixed.EnsureInitialized()) {
    return 1;
  }
  PrintTemperature("calibrated reads", fixed.ReadTemperatureMilliC().value);
  port.state.accuracy_offset_mc = 0;

  // A sensor pulled out of its socket: the input is stuck low.
  port.state.fault = mcp9700_test::Fault::StuckLow;
  const auto pulled = sensor.ReadMeasurement();
  (void)std::printf("pulled out: %s\n", mcp9700::ToString(pulled.error));
  port.state.fault = mcp9700_test::Fault::None;

  // Sleep switches the supply off; a read is NotInitialized until the next start.
  if (const auto s = sensor.Sleep(); !s) {
    return 1;
  }
  (void)std::printf("asleep: %s\n", mcp9700::ToString(sensor.ReadMeasurement().error));
  if (const auto w = sensor.EnsureInitialized(); !w) {
    return 1;
  }
  PrintTemperature("woken, reads", sensor.ReadTemperatureMilliC().value);
  return 0;
}
