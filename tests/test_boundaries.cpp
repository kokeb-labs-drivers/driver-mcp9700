/**
 * @file test_boundaries.cpp
 * @brief The window edges to the millivolt, the saturation threshold, the
 * spread limit, the divider rounding, and the documented limits of `Custom`.
 */
#include "mcp9700.hpp"
#include "test_harness.hpp"
#include "test_util.hpp"

using mcp9700::Config;
using mcp9700::Error;
using mcp9700_test::ScriptPort;
using Sensor = mcp9700::MCP9700<ScriptPort>;

namespace {

Config One(Config c) {
  c.samples = 1;
  c.discard_conversions = 0;
  c.sample_interval_us = 0;
  return c;
}

/// The error of a one-conversion reading of `mv` (the sensor started at a valid
/// 750 mV first).
Error Read(Config c, uint32_t mv) {
  ScriptPort port;
  Sensor sensor(port, One(c));
  if (!sensor.EnsureInitialized().ok()) {
    return Error::InvalidArgument;
  }
  port.Set(mv);
  return sensor.ReadMeasurement().error;
}

Error Read9700(uint32_t mv) {
  return Read(Config{}, mv);
}

} // namespace

TEST_CASE("MCP9700: 20 mV (-48 C) and 1830 mV (+133 C) are accepted, one mV "
          "beyond is not") {
  CHECK_EQ(Read9700(20), Error::Ok);
  CHECK_EQ(Read9700(19), Error::OutputLow);
  CHECK_EQ(Read9700(1830), Error::Ok);
  CHECK_EQ(Read9700(1831), Error::OutputHigh);
}

TEST_CASE("High class accepts 2080 mV (+158 C) and rejects 2081") {
  const Config c = mcp9700::HighTemperaturePreset();
  CHECK_EQ(Read(c, 2080), Error::Ok);
  CHECK_EQ(Read(c, 2081), Error::OutputHigh);
}

TEST_CASE("MCP9701: 49 mV (-18 C) and 2993 mV are accepted, 48 and 2994 are not") {
  const Config c = mcp9700::Mcp9701Preset();
  Config wide = c;
  wide.adc_full_scale_mv = 5000;
  CHECK_EQ(Read(wide, 49), Error::Ok);
  CHECK_EQ(Read(wide, 48), Error::OutputLow);
  CHECK_EQ(Read(wide, 2993), Error::Ok);
  CHECK_EQ(Read(wide, 2994), Error::OutputHigh);
}

TEST_CASE("range_margin_mc 0 puts the edges at -40 C / 100 mV and +125 C / 1750 mV") {
  Config c{};
  c.range_margin_mc = 0;
  CHECK_EQ(Read(c, 100), Error::Ok);
  CHECK_EQ(Read(c, 99), Error::OutputLow);
  CHECK_EQ(Read(c, 1750), Error::Ok);
  CHECK_EQ(Read(c, 1751), Error::OutputHigh);
}

TEST_CASE("Custom 0 mV, 10 mV/C, 0 to 150 C: the part's own minimum is 0 mV "
          "(limit O-12)") {
  Config c = mcp9700_test::CustomConfig(mcp9700_test::chip::kSynthetics[0]);
  c.range_margin_mc = 0;
  CHECK_EQ(Read(c, 1500), Error::Ok);
  CHECK_EQ(Read(c, 1501), Error::OutputHigh);
  // A grounded line reads 0 mV, which is the part's valid minimum: nothing can
  // tell them apart.
  CHECK_EQ(Read(c, 0), Error::Ok);
  c.range_margin_mc = 8000;
  CHECK_EQ(Read(c, 0), Error::Ok);
}

TEST_CASE("Saturation: full scale minus one LSB is the boundary, at every "
          "resolution") {
  Config c{};
  // 12 and 16 bit: LSB 1 mV. 3298 passes the clip check (and is then too hot);
  // 3299 is clipped.
  CHECK_EQ(Read(c, 3298), Error::OutputHigh);
  CHECK_EQ(Read(c, 3299), Error::Saturated);
  c.adc_resolution_bits = 16;
  CHECK_EQ(Read(c, 3298), Error::OutputHigh);
  CHECK_EQ(Read(c, 3299), Error::Saturated);
  c.adc_resolution_bits = 8; // LSB 13 mV
  CHECK_EQ(Read(c, 3286), Error::OutputHigh);
  CHECK_EQ(Read(c, 3287), Error::Saturated);
  CHECK_EQ(Read(c, 0xFFFF'FFFFU), Error::Saturated);
  CHECK_EQ(mcp9700::detail::AdcLsbMv(c), 13U);
}

TEST_CASE("The spread limit: exactly max_spread_mv passes, one more is Unstable") {
  ScriptPort port;
  Config c{};
  c.samples = 2;
  c.discard_conversions = 0;
  Sensor sensor(port, c);
  REQUIRE(sensor.EnsureInitialized().ok());
  port.values[0] = 750;
  port.values[1] = 780;
  port.count = 2;
  port.next = 0;
  const auto ok = sensor.ReadMeasurement();
  CHECK(ok.ok());
  CHECK_EQ(ok.value.spread_mv, 30U);
  CHECK_EQ(ok.value.output_mv, 765U);
  port.values[1] = 781;
  port.next = 0;
  CHECK_EQ(sensor.ReadMeasurement().error, Error::Unstable);
}

TEST_CASE("Dividers: 2/1 and 3/2 scale the pin back to VOUT and round half "
          "away from zero") {
  ScriptPort port;
  Config c{};
  c.divider_num = 2;
  c.divider_den = 1;
  Sensor a(port, One(c));
  port.Set(375);
  REQUIRE(a.EnsureInitialized().ok());
  auto m = a.ReadMeasurement();
  REQUIRE(m.ok());
  CHECK_EQ(m.value.output_mv, 750U);
  CHECK_EQ(m.value.temperature_mc, 25'000);
  c.divider_num = 3;
  c.divider_den = 2;
  Sensor b(port, One(c));
  port.Set(500);
  REQUIRE(b.EnsureInitialized().ok());
  CHECK_EQ(b.ReadMeasurement().value.output_mv, 750U);
  port.Set(501); // 751.5 rounds to 752
  m = b.ReadMeasurement();
  CHECK_EQ(m.value.output_mv, 752U);
  CHECK_EQ(m.value.temperature_mc, 25'150);
}

TEST_CASE("Extremes: 64 samples, divider 65535 and a 10 V full scale do not "
          "overflow") {
  ScriptPort port;
  Config c{};
  c.samples = 64;
  c.discard_conversions = 0;
  c.sample_interval_us = 0;
  c.divider_num = 65'535;
  c.divider_den = 65'535;
  c.adc_full_scale_mv = 10'000;
  Sensor sensor(port, c);
  port.Set(750);
  REQUIRE(sensor.EnsureInitialized().ok());
  CHECK_EQ(sensor.ReadTemperatureMilliC().value, 25'000);
  c.divider_den = 1;
  Sensor big(port, c);
  port.Set(9998);
  CHECK_EQ(big.EnsureInitialized().error, Error::DeviceIdMismatch);
  Config n = c;
  n.divider_num = 1;
  Sensor tiny(port, n);
  port.Set(750);
  REQUIRE(tiny.EnsureInitialized().ok());
}

TEST_CASE("ResolutionMilliC: one ADC step at the slope, rounded up") {
  ScriptPort port;
  Sensor a(port);
  CHECK_EQ(a.ResolutionMilliC(), 81U);
  Config c = mcp9700::Mcp9701Preset();
  Sensor b(port, c);
  CHECK_EQ(b.ResolutionMilliC(), 42U);
  c.adc_resolution_bits = 16;
  Sensor d(port, c);
  CHECK_EQ(d.ResolutionMilliC(), 3U);
  CHECK_EQ(a.GetSpec().value.v0_mv, 500U);
}

TEST_CASE("The ground-fault floor of the default window, per variant, as documented") {
  // The lowest conversion accepted is v0 + tc x (minimum - 8 C): a VOUT shorted to ground is
  // recognised only when the ADC reports below it (docs/configuration.md, "The ground-fault
  // floor").
  const uint32_t floor_mv[] = {20, 20, 20, 49, 49};
  for (std::size_t i = 0; i < 5; ++i) {
    Config c = mcp9700_test::NamedConfig(i);
    c.adc_full_scale_mv = 5000;
    CHECK_EQ(Read(c, floor_mv[i]), Error::Ok);
    CHECK_EQ(Read(c, floor_mv[i] - 1U), Error::OutputLow);
    CHECK_EQ(Read(c, 0), Error::OutputLow);
  }
}

TEST_CASE(
    "A smaller margin lifts the floor: 2 C gives 80 mV on an MCP9700 and 166 mV on an MCP9701") {
  Config c{};
  c.range_margin_mc = 2000;
  CHECK_EQ(Read(c, 80), Error::Ok);
  CHECK_EQ(Read(c, 79), Error::OutputLow);
  Config d = mcp9700::Mcp9701Preset();
  d.adc_full_scale_mv = 5000;
  d.range_margin_mc = 2000;
  CHECK_EQ(Read(d, 166), Error::Ok);
  CHECK_EQ(Read(d, 165), Error::OutputLow);
}

TEST_CASE("A stable mid-scale level passes every check (the floating-line hazard, documented)") {
  // 1650 mV held on the pin reads as 115 C on a 10 mV/C part: nothing can tell it from a sensor.
  CHECK_EQ(Read9700(1650), Error::Ok);
  Config c{};
  c.discard_conversions = 1;
  ScriptPort port;
  port.Set(1650);
  Sensor sensor(port, c);
  CHECK(sensor.EnsureInitialized().ok());
}
