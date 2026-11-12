/**
 * @file test_calibration.cpp
 * @brief The one-point calibration offset (Section 4.1, p.10): added after the
 * window check.
 */
#include "mcp9700.hpp"
#include "test_harness.hpp"
#include "test_util.hpp"

using mcp9700::Error;
using mcp9700_test::MinimalPort;

TEST_CASE("offset_cal_mc corrects a part that reads 3 C high") {
  MinimalPort port;
  port.state.accuracy_offset_mc = 3000;
  mcp9700::Config c{};
  mcp9700::MCP9700<MinimalPort> raw(port, c);
  REQUIRE(raw.EnsureInitialized().ok());
  const auto before = raw.ReadTemperatureMilliC();
  REQUIRE(before.ok());
  CHECK(before.value > 27'900 && before.value < 28'100);
  // The one-point calibration: offset = reference - reading.
  c.offset_cal_mc = 25'000 - before.value;
  mcp9700::MCP9700<MinimalPort> fixed(port, c);
  REQUIRE(fixed.EnsureInitialized().ok());
  const auto after = fixed.ReadTemperatureMilliC();
  REQUIRE(after.ok());
  CHECK(after.value > 24'900 && after.value < 25'100);
}

TEST_CASE("The calibration is added after the window check: the result may "
          "leave the window") {
  mcp9700_test::ScriptPort port;
  mcp9700::Config c{};
  c.samples = 1;
  c.discard_conversions = 0;
  c.offset_cal_mc = -20'000;
  mcp9700::MCP9700<mcp9700_test::ScriptPort> sensor(port, c);
  REQUIRE(sensor.EnsureInitialized().ok());
  port.Set(20); // -48.000 C, the window edge
  const auto m = sensor.ReadTemperatureMilliC();
  REQUIRE(m.ok());
  CHECK_EQ(m.value, -68'000);
  port.Set(19);
  CHECK_EQ(sensor.ReadTemperatureMilliC().error, Error::OutputLow);
}
