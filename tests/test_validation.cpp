/**
 * @file test_validation.cpp
 * @brief `Validate()` at every bound: the value at the limit is accepted, one
 * beyond is rejected.
 */
#include "mcp9700.hpp"
#include "reference_mcp9700.hpp"
#include "test_harness.hpp"
#include "test_util.hpp"

using mcp9700::Config;
using mcp9700::Error;
using mcp9700_test::MinimalPort;

namespace {

/// Accept `ok_value` and reject `bad_value` for the field set by `set`, and
/// keep the oracle agreeing.
template <typename F, typename T>
void Edge(F set, T ok_value, T bad_value) {
  Config good{};
  set(good, ok_value);
  CHECK_EQ(mcp9700::Validate(good), Error::Ok);
  CHECK(mcp9700_ref::Valid(good));
  Config bad{};
  set(bad, bad_value);
  CHECK_EQ(mcp9700::Validate(bad), Error::InvalidArgument);
  CHECK(!mcp9700_ref::Valid(bad));
}

} // namespace

TEST_CASE("Every numeric field is checked at its limit") {
  Edge([](Config& c, uint32_t v) { c.adc_full_scale_mv = v; }, 100U, 99U);
  Edge([](Config& c, uint32_t v) { c.adc_full_scale_mv = v; }, 10'000U, 10'001U);
  Edge([](Config& c, uint8_t v) { c.adc_resolution_bits = v; }, uint8_t{8}, uint8_t{7});
  Edge([](Config& c, uint8_t v) { c.adc_resolution_bits = v; }, uint8_t{16}, uint8_t{17});
  Edge([](Config& c, uint16_t v) { c.divider_num = v; }, uint16_t{1}, uint16_t{0});
  Edge([](Config& c, uint16_t v) { c.divider_den = v; }, uint16_t{65'535}, uint16_t{0});
  Edge([](Config& c, int32_t v) { c.offset_cal_mc = v; }, 20'000, 20'001);
  Edge([](Config& c, int32_t v) { c.offset_cal_mc = v; }, -20'000, -20'001);
  Edge([](Config& c, int32_t v) { c.range_margin_mc = v; }, 20'000, 20'001);
  Edge([](Config& c, int32_t v) { c.range_margin_mc = v; }, 0, -1);
  Edge([](Config& c, uint16_t v) { c.samples = v; }, uint16_t{64}, uint16_t{65});
  Edge([](Config& c, uint16_t v) { c.samples = v; }, uint16_t{1}, uint16_t{0});
  Edge(
      [](Config& c, uint32_t v) {
        c.sample_interval_us = v;
        c.samples = 1;
        c.discard_conversions = 0;
      },
      100'000U, 100'001U);
  Edge(
      [](Config& c, uint8_t v) {
        c.discard_conversions = v;
        c.sample_interval_us = 0;
      },
      uint8_t{4}, uint8_t{5});
  Edge([](Config& c, uint32_t v) { c.max_spread_mv = v; }, 1000U, 1001U);
  Edge([](Config& c, uint32_t v) { c.power_on_settle_us = v; }, 1'000'000U, 1'000'001U);
}

TEST_CASE("The averaging window is capped at 1 000 000 us, exactly") {
  Config c{};
  c.discard_conversions = 0;
  c.samples = 11; // 10 gaps
  c.sample_interval_us = 100'000;
  CHECK_EQ(mcp9700::Validate(c), Error::Ok);
  CHECK(mcp9700_ref::Valid(c));
  c.samples = 12; // 11 gaps: 1 100 000 us
  CHECK_EQ(mcp9700::Validate(c), Error::InvalidArgument);
  CHECK(!mcp9700_ref::Valid(c));
  c.samples = 64;
  c.discard_conversions = 0;
  c.sample_interval_us = 15'873; // 63 gaps: 999 999 us
  CHECK_EQ(mcp9700::Validate(c), Error::Ok);
  c.sample_interval_us = 15'874; // 1 000 062 us
  CHECK_EQ(mcp9700::Validate(c), Error::InvalidArgument);
  c.samples = 63;
  c.discard_conversions = 1; // 63 gaps again
  c.sample_interval_us = 15'873;
  CHECK_EQ(mcp9700::Validate(c), Error::Ok);
  c.discard_conversions = 2;
  c.sample_interval_us = 15'874;
  CHECK_EQ(mcp9700::Validate(c), Error::InvalidArgument);
}

TEST_CASE("A value that is not an enumerator is rejected, and a 9701 cannot be "
          "High") {
  Config c{};
  c.variant = mcp9700_test::RawEnum<mcp9700::Variant>(6);
  CHECK_EQ(mcp9700::Validate(c), Error::InvalidArgument);
  c = Config{};
  c.variant = mcp9700_test::RawEnum<mcp9700::Variant>(255);
  CHECK_EQ(mcp9700::Validate(c), Error::InvalidArgument);
  c = Config{};
  c.range = mcp9700_test::RawEnum<mcp9700::TemperatureClass>(2);
  CHECK_EQ(mcp9700::Validate(c), Error::InvalidArgument);
  c = mcp9700::Mcp9701Preset();
  c.range = mcp9700::TemperatureClass::High;
  CHECK_EQ(mcp9700::Validate(c), Error::InvalidArgument);
  CHECK(!mcp9700_ref::Valid(c));
}

TEST_CASE("An invalid configuration fails at EnsureInitialized with zero port "
          "traffic") {
  MinimalPort port;
  Config c{};
  c.samples = 0;
  mcp9700::MCP9700<MinimalPort> sensor(port, c);
  CHECK_EQ(sensor.EnsureInitialized().error, Error::InvalidArgument);
  CHECK_EQ(port.state.transfers, 0);
  CHECK_EQ(port.state.bus_up_calls, 0);
  CHECK_EQ(port.state.delay_calls, 0);
  CHECK_EQ(sensor.GetSpec().error, Error::InvalidArgument);
  CHECK_EQ(sensor.ResolutionMilliC(), 0U);
}

TEST_CASE("The presets are valid, and Config{} is the datasheet typical MCP9700") {
  CHECK_EQ(mcp9700::Validate(mcp9700::FastPreset()), Error::Ok);
  CHECK_EQ(mcp9700::Validate(mcp9700::LowNoisePreset()), Error::Ok);
  CHECK_EQ(mcp9700::FastPreset().samples, 1);
  CHECK_EQ(mcp9700::LowNoisePreset().samples, 32);
  CHECK(mcp9700::HighTemperaturePreset().range == mcp9700::TemperatureClass::High);
  CHECK(mcp9700::Mcp9701Preset().variant == mcp9700::Variant::Mcp9701);
  const Config c{};
  CHECK(c.variant == mcp9700::Variant::Mcp9700);
  CHECK_EQ(c.adc_full_scale_mv, 3300U);
  CHECK_EQ(c.adc_resolution_bits, 12);
  CHECK_EQ(c.divider_num, 1);
  CHECK_EQ(c.range_margin_mc, 8000);
  CHECK_EQ(c.power_on_settle_us, 2400U);
}

TEST_CASE("Validate agrees with the oracle over a grid of configurations") {
  for (int i = 0; i < 4000; ++i) {
    Config c = (i % 7 == 0) ? mcp9700_test::ValidCustom()
                            : mcp9700_test::NamedConfig(static_cast<std::size_t>(i % 5));
    c.samples = static_cast<uint16_t>((i * 7) % 70);
    c.discard_conversions = static_cast<uint8_t>((i * 3) % 7);
    c.sample_interval_us = static_cast<uint32_t>((i * 4001) % 60'000);
    c.divider_den = static_cast<uint16_t>(i % 3);
    c.range =
        ((i % 11) == 0) ? mcp9700::TemperatureClass::High : mcp9700::TemperatureClass::Extended;
    c.custom.accuracy_max_mc += (i % 13 == 0) ? 30'000 : 0;
    CHECK_EQ(mcp9700::Validate(c) == Error::Ok, mcp9700_ref::Valid(c));
  }
}
