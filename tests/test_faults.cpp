/**
 * @file test_faults.cpp
 * @brief Each fault mode of the model maps to its error; bus failures; the
 * precedence rule.
 */
#include "mcp9700.hpp"
#include "test_harness.hpp"
#include "test_util.hpp"

using mcp9700::Error;
using mcp9700_test::Fault;
using mcp9700_test::FullPort;
using mcp9700_test::MinimalPort;
using Sensor = mcp9700::MCP9700<MinimalPort>;

namespace {
Error ReadWith(MinimalPort& port, Sensor& sensor, Fault fault) {
  port.state.fault = fault;
  return sensor.ReadMeasurement().error;
}
} // namespace

TEST_CASE("Stuck, floating and noisy lines are errors, never data") {
  MinimalPort port;
  Sensor sensor(port);
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK_EQ(ReadWith(port, sensor, Fault::StuckLow), Error::OutputLow);
  CHECK_EQ(ReadWith(port, sensor, Fault::FloatHigh), Error::Saturated);
  port.state.drift_step_mv = 100.0;
  CHECK_EQ(ReadWith(port, sensor, Fault::FloatingDrift), Error::Unstable);
  port.state.spike_every = 4;
  CHECK_EQ(ReadWith(port, sensor, Fault::Spike), Error::Unstable);
  CHECK_EQ(ReadWith(port, sensor, Fault::None), Error::Ok);
}

TEST_CASE("A floating input that holds a plausible value is NOT detected "
          "(documented limit)") {
  MinimalPort port;
  mcp9700::Config c{};
  c.channel = 3; // unwired
  port.state.fault = Fault::FloatingHold;
  Sensor sensor(port, c);
  CHECK(sensor.EnsureInitialized().ok());
  const auto m = sensor.ReadMeasurement();
  CHECK(m.ok());
  CHECK_EQ(m.value.output_mv, 800U);
}

TEST_CASE("Start-up maps every output fault to DeviceIdMismatch") {
  const Fault faults[] = {Fault::StuckLow, Fault::FloatHigh, Fault::FloatingDrift, Fault::Spike};
  for (const Fault f : faults) {
    FullPort port;
    port.state.fault = f;
    port.state.drift_step_mv = 100.0;
    port.state.spike_every = 4;
    mcp9700::MCP9700<FullPort> sensor(port);
    CHECK_EQ(sensor.EnsureInitialized().error, Error::DeviceIdMismatch);
    CHECK(!port.state.powered);
    CHECK_EQ(port.errors, 1);
  }
  FullPort hot;
  hot.state.temperature_mc = 200'000; // OutputHigh at start-up
  mcp9700::MCP9700<FullPort> sensor(hot);
  CHECK_EQ(sensor.EnsureInitialized().error, Error::DeviceIdMismatch);
}

TEST_CASE("The first conversion after a channel switch is wrong, and discard "
          "removes it") {
  for (const uint8_t discard : {uint8_t{0}, uint8_t{1}}) {
    MinimalPort port;
    port.state.hold_ratio = 0.5;
    mcp9700::Config c{};
    c.discard_conversions = discard;
    Sensor sensor(port, c);
    REQUIRE(sensor.EnsureInitialized().ok());
    uint32_t other = 0;
    REQUIRE(port.state.Convert(1, other)); // another driver used channel 1 meanwhile
    const auto m = sensor.ReadMeasurement();
    if (discard == 0U) {
      CHECK_EQ(m.error, Error::Unstable); // 375 mV among 750 mV
    } else {
      CHECK(m.ok());
      CHECK_EQ(m.value.output_mv, 750U);
    }
  }
}

TEST_CASE("A clip below the declared full scale is Saturated only when the "
          "declaration says so") {
  MinimalPort port;
  port.state.temperature_mc = 85'000; // 1350 mV
  port.state.clip_mv = 1100.0;
  mcp9700::Config c{};
  c.adc_full_scale_mv = 1100;
  Sensor right(port, c);
  port.state.temperature_mc = 25'000;
  REQUIRE(right.EnsureInitialized().ok());
  port.state.temperature_mc = 85'000;
  CHECK_EQ(right.ReadMeasurement().error, Error::Saturated);
  Sensor wrong(port);
  port.state.temperature_mc = 25'000;
  REQUIRE(wrong.EnsureInitialized().ok());
  port.state.temperature_mc = 85'000;
  const auto m = wrong.ReadMeasurement();
  REQUIRE(m.ok()); // the wrong declaration hides the clip: 1100 mV reads as 60 C
  CHECK_EQ(m.value.temperature_mc, 60'000);
}

TEST_CASE("Precedence: Saturated beats Unstable beats the window") {
  mcp9700_test::ScriptPort port;
  mcp9700::Config c{};
  c.samples = 3;
  c.discard_conversions = 0;
  mcp9700::MCP9700<mcp9700_test::ScriptPort> sensor(port, c);
  REQUIRE(sensor.EnsureInitialized().ok());
  const uint32_t sat[3] = {3299, 100, 100};
  const uint32_t unstable_low[3] = {0, 100, 500};
  const uint32_t low[3] = {0, 0, 0};
  const uint32_t high_unstable[3] = {1900, 1990, 1990};
  auto run = [&](const uint32_t (&v)[3]) {
    for (std::size_t i = 0; i < 3; ++i) {
      port.values[i] = v[i];
    }
    port.count = 3;
    port.next = 0;
    return sensor.ReadMeasurement().error;
  };
  CHECK_EQ(run(sat), Error::Saturated);
  CHECK_EQ(run(unstable_low), Error::Unstable);
  CHECK_EQ(run(low), Error::OutputLow);
  CHECK_EQ(run(high_unstable), Error::Unstable);
}

TEST_CASE("A failure of conversion k is Bus, stops there, and leaves the next "
          "read intact") {
  const int total = 1 + 8;
  for (int k = 0; k < total; ++k) {
    MinimalPort port;
    Sensor sensor(port);
    REQUIRE(sensor.EnsureInitialized().ok());
    const int before = port.state.transfers;
    port.state.fail_transfer = before + k;
    CHECK_EQ(sensor.ReadMeasurement().error, Error::Bus);
    CHECK_EQ(port.state.transfers, before + k + 1);
    port.state.fail_transfer = -1;
    CHECK(sensor.ReadMeasurement().ok());
  }
}

TEST_CASE("Bus failures at start-up: ADC bring-up, pin write, conversion, "
          "switch-off") {
  {
    FullPort port;
    port.state.bus_up_ok = false;
    mcp9700::MCP9700<FullPort> sensor(port);
    CHECK_EQ(sensor.EnsureInitialized().error, Error::Bus);
    CHECK_EQ(port.state.transfers, 0);
    CHECK_EQ(port.gpio_calls, 0);
    CHECK_EQ(port.errors, 1);
  }
  {
    FullPort port;
    port.gpio_fail_at = 0;
    mcp9700::MCP9700<FullPort> sensor(port);
    CHECK_EQ(sensor.EnsureInitialized().error, Error::Bus);
    CHECK_EQ(port.state.transfers, 0);
    CHECK(!sensor.IsInitialized());
    // A failed write may still have powered the sensor: a best-effort switch-off follows.
    CHECK_EQ(port.gpio_calls, 2);
    CHECK_EQ(port.gpio_inactive, 1);
    CHECK(!port.state.powered);
  }
  {
    FullPort port;
    port.state.fail_transfer = 3;
    mcp9700::MCP9700<FullPort> sensor(port);
    CHECK_EQ(sensor.EnsureInitialized().error, Error::Bus);
    CHECK_EQ(port.state.transfers, 4);
    CHECK_EQ(port.gpio_inactive, 1); // powered off again
  }
  {
    FullPort port; // the sensor cannot be switched off after a failed check:
                   // still the original error
    port.state.fault = Fault::StuckLow;
    port.gpio_fail_at = 1;
    mcp9700::MCP9700<FullPort> sensor(port);
    CHECK_EQ(sensor.EnsureInitialized().error, Error::DeviceIdMismatch);
    CHECK_EQ(port.warnings, 1);
  }
  {
    FullPort port;
    mcp9700::MCP9700<FullPort> sensor(port);
    REQUIRE(sensor.EnsureInitialized().ok());
    port.gpio_ok = false;
    CHECK_EQ(sensor.Deinitialize().error, Error::Bus);
    CHECK(!sensor.IsInitialized());
    port.gpio_ok = true;
    CHECK(sensor.EnsureInitialized().ok());
    port.gpio_ok = false;
    CHECK_EQ(sensor.Sleep().error, Error::Bus);
    CHECK_EQ(sensor.EnsureInitialized().error,
             Error::Bus); // pin write of the wake fails
  }
}

TEST_CASE("A failed switch-off is retried by EnsureInitialized() and Sleep(), as documented") {
  FullPort port;
  mcp9700::MCP9700<FullPort> sensor(port);
  REQUIRE(sensor.EnsureInitialized().ok());
  port.gpio_ok = false;
  CHECK_EQ(sensor.Sleep().error, Error::Bus);
  CHECK(!sensor.IsInitialized());
  CHECK(sensor.Deinitialize().ok()); // nothing to do: it does not retry the write
  CHECK(port.state.powered);         // the sensor is still powered
  port.gpio_ok = true;
  REQUIRE(sensor.EnsureInitialized().ok());
  REQUIRE(sensor.Sleep().ok());
  CHECK(!port.state.powered);
}
