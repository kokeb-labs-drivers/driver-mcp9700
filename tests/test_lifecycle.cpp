/**
 * @file test_lifecycle.cpp
 * @brief Start, stop, restart: the constructor, `EnsureInitialized`,
 * `Deinitialize`, `Sleep`.
 */
#include "mcp9700.hpp"
#include "test_harness.hpp"
#include "test_util.hpp"

using mcp9700::Error;
using mcp9700_test::FullPort;
using mcp9700_test::MinimalPort;

TEST_CASE("The constructor does no I/O and every call returns NotInitialized "
          "before the start") {
  FullPort port;
  mcp9700::MCP9700<FullPort> sensor(port);
  CHECK_EQ(port.state.transfers, 0);
  CHECK_EQ(sensor.ReadMeasurement().error, Error::NotInitialized);
  CHECK_EQ(sensor.ReadTemperatureMilliC().error, Error::NotInitialized);
  CHECK_EQ(sensor.ReadOutputMv().error, Error::NotInitialized);
  CHECK_EQ(sensor.ReadRawPortMv().error, Error::NotInitialized);
  CHECK_EQ(sensor.Sleep().error, Error::NotInitialized);
  CHECK(sensor.Deinitialize().ok());
  CHECK_EQ(port.state.transfers, 0);
  CHECK_EQ(port.state.bus_up_calls, 0);
  CHECK_EQ(port.gpio_calls, 0);
  CHECK(!sensor.IsInitialized());
  CHECK_EQ(sensor.GetLastRawPortMv(), 0U);
  CHECK(sensor.GetConfig().variant == mcp9700::Variant::Mcp9700);
}

TEST_CASE("EnsureInitialized powers the sensor, waits, reads once, then is a no-op") {
  FullPort port;
  mcp9700::MCP9700<FullPort> sensor(port);
  CHECK(!port.state.powered);
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK(port.state.powered);
  CHECK_EQ(port.gpio_active, 1);
  CHECK_EQ(port.state.transfers, 9); // 1 dummy + 8 samples
  const int transfers = port.state.transfers;
  for (int i = 0; i < 3; ++i) {
    CHECK(sensor.EnsureInitialized().ok());
  }
  CHECK_EQ(port.state.transfers, transfers);
  CHECK_EQ(port.state.bus_up_calls, 1);
  CHECK_EQ(port.gpio_calls, 1);
}

TEST_CASE("A failed start stays uninitialised, powers the sensor off and can "
          "be retried") {
  FullPort port;
  port.state.wired_channel = 3; // nothing on channel 0
  mcp9700::MCP9700<FullPort> sensor(port);
  CHECK_EQ(sensor.EnsureInitialized().error, Error::DeviceIdMismatch);
  CHECK(!sensor.IsInitialized());
  CHECK(!port.state.powered);
  CHECK(!port.last_level_active);
  CHECK_EQ(port.gpio_inactive, 1);
  CHECK_EQ(port.errors, 1);
  CHECK_EQ(sensor.ReadMeasurement().error, Error::NotInitialized);
  port.state.wired_channel = 0;
  REQUIRE(sensor.EnsureInitialized().ok());
}

TEST_CASE("Deinitialize switches off first and then every read is "
          "NotInitialized; Sleep and wake") {
  FullPort port;
  mcp9700::MCP9700<FullPort> sensor(port);
  REQUIRE(sensor.EnsureInitialized().ok());
  REQUIRE(sensor.Sleep().ok());
  CHECK(!port.state.powered);
  CHECK(!sensor.IsInitialized());
  const int transfers = port.state.transfers;
  CHECK_EQ(sensor.ReadMeasurement().error, Error::NotInitialized);
  CHECK_EQ(port.state.transfers, transfers);
  REQUIRE(sensor.EnsureInitialized().ok()); // wake: the start-up check runs again
  CHECK(port.state.powered);
  CHECK(port.state.transfers > transfers);
  REQUIRE(sensor.Deinitialize().ok());
  CHECK(sensor.Deinitialize().ok()); // idempotent, no traffic
  CHECK_EQ(port.gpio_inactive, 2);
}

TEST_CASE("Sleep without a GpioSet hook is Unsupported and keeps the driver "
          "running") {
  MinimalPort port;
  mcp9700::MCP9700<MinimalPort> sensor(port);
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK_EQ(sensor.Sleep().error, Error::Unsupported);
  CHECK(sensor.IsInitialized());
  CHECK(sensor.Deinitialize().ok());
  CHECK(!sensor.IsInitialized());
}

namespace {
/// A port that declares `GpioSet()` with no pin behind it, succeeding as a no-op. It documents what
/// the driver does with such a port; the ESP32 example avoids the case with two port types, and
/// `tests/esp32_port_check.cpp` guards that.
class NoPinPort : public mcp9700_test::MockPortBase<NoPinPort> {
public:
  int calls = 0;
  void DelayUs(uint32_t us) noexcept {
    state.Advance(us);
  }
  bool GpioSet(mcp9700::CtrlPin pin, mcp9700::GpioSignal signal) noexcept {
    (void)pin;
    (void)signal;
    calls++;
    return true;
  }
};
} // namespace

TEST_CASE("A GpioSet hook with no pin behind it (a no-op returning true) starts and sleeps") {
  NoPinPort port;
  mcp9700::MCP9700<NoPinPort> sensor(port);
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK(sensor.ReadMeasurement().ok());
  CHECK(sensor.Sleep().ok());
  CHECK_EQ(port.calls, 2); // switch on, switch off
  CHECK(sensor.EnsureInitialized().ok());
}

TEST_CASE("A GpioSet hook that returns false with no pin fails every start-up with Bus") {
  // The mistake an example port must not make: declaring GpioSet() but refusing it when no pin is
  // configured. The driver cannot tell a missing pin from a broken one.
  FullPort port;
  port.gpio_ok = false;
  mcp9700::MCP9700<FullPort> sensor(port);
  CHECK_EQ(sensor.EnsureInitialized().error, Error::Bus);
  CHECK_EQ(sensor.EnsureInitialized().error, Error::Bus);
}

TEST_CASE("A failed read does not uninitialise; the sensor can recover") {
  MinimalPort port;
  mcp9700::MCP9700<MinimalPort> sensor(port);
  REQUIRE(sensor.EnsureInitialized().ok());
  port.state.fault = mcp9700_test::Fault::StuckLow;
  CHECK_EQ(sensor.ReadMeasurement().error, Error::OutputLow);
  CHECK(sensor.IsInitialized());
  port.state.fault = mcp9700_test::Fault::None;
  CHECK(sensor.ReadMeasurement().ok());
}

TEST_CASE("A supply that drops mid-run reads as OutputLow, not as a temperature") {
  FullPort port;
  mcp9700::MCP9700<FullPort> sensor(port);
  REQUIRE(sensor.EnsureInitialized().ok());
  port.state.SetPower(false);
  CHECK_EQ(sensor.ReadMeasurement().error, Error::OutputLow);
  port.state.SetPower(true);
  CHECK(!sensor.ReadMeasurement().ok()); // t_ON passes during the reading: the
                                         // first conversions are low
  port.state.Advance(1000);
  CHECK(sensor.ReadMeasurement().ok());
}

TEST_CASE("A Custom start logs its one warning, a named one does not") {
  FullPort custom_port;
  mcp9700::MCP9700<FullPort> custom(custom_port, mcp9700_test::ValidCustom());
  custom_port.state.UseSynthetic(mcp9700_test::chip::kSynthetics[1]);
  REQUIRE(custom.EnsureInitialized().ok());
  CHECK_EQ(custom_port.warnings, 1);
  FullPort named_port;
  mcp9700::MCP9700<FullPort> named(named_port);
  REQUIRE(named.EnsureInitialized().ok());
  CHECK_EQ(named_port.warnings, 0);
  mcp9700::Config bad = mcp9700_test::ValidCustom();
  bad.custom.tc_uv_per_c = 0;
  FullPort bad_port;
  mcp9700::MCP9700<FullPort> invalid(bad_port, bad);
  CHECK_EQ(invalid.EnsureInitialized().error, Error::InvalidArgument);
  CHECK_EQ(bad_port.warnings, 0);
}
