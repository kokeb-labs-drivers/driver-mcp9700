/**
 * @file esp32_port_check.cpp
 * @brief The ESP32 example ports compiled on the host against stub ESP-IDF headers: the port for a
 * sensor on the supply declares no `GpioSet()` and starts; the switched port needs a pin and says
 * `Bus` without one.
 *
 * This guards the shape of the example (the bug it prevents: a port that declares `GpioSet()` and
 * refuses it when no pin is set, which makes every start-up fail). It does not test ESP-IDF.
 */
#include "esp32_mcp9700_port.hpp"
#include "mcp9700.hpp"
#include "test_harness.hpp"

using mcp9700::Error;

namespace {
mcp9700::Config ExampleConfig() {
  mcp9700::Config c{};
  c.adc_full_scale_mv = Esp32Mcp9700Port::kAdcFullScaleMv;
  c.adc_resolution_bits = 12;
  return c;
}
} // namespace

TEST_CASE("The unswitched ESP32 port declares no GpioSet, starts, reads and cannot sleep") {
  static_assert(!mcp9700::detail::HasGpioSet<Esp32Mcp9700Port>,
                "an unswitched port must not declare GpioSet()");
  Esp32Mcp9700Port port;
  mcp9700::MCP9700<Esp32Mcp9700Port> sensor(port, ExampleConfig());
  REQUIRE(sensor.EnsureInitialized().ok());
  const auto reading = sensor.ReadTemperatureMilliC();
  REQUIRE(reading.ok());
  CHECK_EQ(reading.value, 25'000);
  CHECK_EQ(sensor.Sleep().error, Error::Unsupported);
}

TEST_CASE("The switched ESP32 port without a pin fails with Bus; with a pin it starts, powers and "
          "sleeps") {
  static_assert(mcp9700::detail::HasGpioSet<Esp32Mcp9700SwitchedPort>,
                "a switched port declares GpioSet()");
  {
    Esp32Mcp9700SwitchedPort port{Esp32Mcp9700Pins{}};
    mcp9700::MCP9700<Esp32Mcp9700SwitchedPort> sensor(port, ExampleConfig());
    CHECK_EQ(sensor.EnsureInitialized().error, Error::Bus);
  }
  Esp32Mcp9700Pins pins{};
  pins.enable = 4;
  Esp32Mcp9700SwitchedPort port{pins};
  mcp9700::MCP9700<Esp32Mcp9700SwitchedPort> sensor(port, ExampleConfig());
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK_EQ(g_stub_gpio_level, 1U); // powered
  REQUIRE(sensor.Sleep().ok());
  CHECK_EQ(g_stub_gpio_level, 0U); // cut
}
