/**
 * @file test_timing.cpp
 * @brief The waits: power-on settle (the only protection against t_ON), sample
 * intervals, traffic.
 */
#include "mcp9700.hpp"
#include "test_harness.hpp"
#include "test_util.hpp"

using mcp9700::Error;
using mcp9700_test::FullPort;
using mcp9700_test::MinimalPort;

TEST_CASE("Start-up waits at least power_on_settle_us and no single wait is "
          "longer than needed") {
  FullPort port;
  mcp9700::MCP9700<FullPort> sensor(port);
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK(port.state.delayed_us >= mcp9700::Config{}.power_on_settle_us);
  CHECK_EQ(port.state.max_delay_us, 2400U);
  CHECK(mcp9700::Config{}.power_on_settle_us >= 3U * mcp9700::kPowerOnTypicalUs);
}

TEST_CASE("The settle knob is the only protection: t_ON 3000 us fails at 2400 "
          "and passes at 4000") {
  FullPort short_wait;
  short_wait.state.turn_on_us = 3000.0;
  mcp9700::MCP9700<FullPort> a(short_wait);
  CHECK_EQ(a.EnsureInitialized().error, Error::DeviceIdMismatch);
  FullPort long_wait;
  long_wait.state.turn_on_us = 3000.0;
  mcp9700::Config c{};
  c.power_on_settle_us = 4000;
  mcp9700::MCP9700<FullPort> b(long_wait, c);
  CHECK(b.EnsureInitialized().ok());
}

TEST_CASE("A settle of 0 is a valid declaration and still calls DelayUs once") {
  MinimalPort port;
  mcp9700::Config c{};
  c.power_on_settle_us = 0;
  c.samples = 1;
  c.discard_conversions = 0;
  mcp9700::MCP9700<MinimalPort> sensor(port, c);
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK_EQ(port.state.delay_calls, 1);
}

TEST_CASE("Intervals: discard + samples - 1 waits, none after the last, none "
          "when the interval is 0") {
  struct Shape {
    uint16_t samples;
    uint8_t discard;
    uint32_t interval;
  };
  const Shape shapes[] = {{1, 0, 100}, {1, 1, 100}, {8, 1, 100}, {64, 4, 50}, {8, 1, 0}, {1, 0, 0}};
  for (const Shape& s : shapes) {
    MinimalPort port;
    mcp9700::Config c{};
    c.samples = s.samples;
    c.discard_conversions = s.discard;
    c.sample_interval_us = s.interval;
    mcp9700::MCP9700<MinimalPort> sensor(port, c);
    REQUIRE(sensor.EnsureInitialized().ok());
    port.state.delay_calls = 0;
    port.state.delayed_us = 0;
    const int before = port.state.transfers;
    REQUIRE(sensor.ReadMeasurement().ok());
    const int total = static_cast<int>(s.samples) + static_cast<int>(s.discard);
    CHECK_EQ(port.state.transfers - before, total);
    const int waits = (s.interval == 0U) ? 0 : (total - 1);
    CHECK_EQ(port.state.delay_calls, waits);
    CHECK_EQ(port.state.delayed_us, static_cast<uint64_t>(waits) * s.interval);
  }
}

TEST_CASE("ReadOutputMv and ReadTemperatureMilliC send exactly one reading's "
          "traffic") {
  MinimalPort port;
  mcp9700::MCP9700<MinimalPort> sensor(port);
  REQUIRE(sensor.EnsureInitialized().ok());
  int before = port.state.transfers;
  REQUIRE(sensor.ReadOutputMv().ok());
  CHECK_EQ(port.state.transfers - before, 9);
  before = port.state.transfers;
  REQUIRE(sensor.ReadTemperatureMilliC().ok());
  CHECK_EQ(port.state.transfers - before, 9);
}
