/**
 * @file test_model.cpp
 * @brief End to end over the sensor and ADC model: every named variant and a
 * set of synthetic `Custom` parts, swept across temperature, against the model
 * and against the oracle.
 */
#include <cmath>

#include "mcp9700.hpp"
#include "reference_mcp9700.hpp"
#include "test_harness.hpp"
#include "test_util.hpp"

using mcp9700::Error;
using mcp9700_test::MinimalPort;
using Sensor = mcp9700::MCP9700<MinimalPort>;

namespace {

struct Part {
  mcp9700::Config config{};
  double v0 = 0.0;
  double tc = 0.0;
  double min_c = 0.0;
  double max_c = 0.0;
};

/// All parts under test: the five named rows and the synthetic customs.
std::size_t Parts(Part* out) {
  std::size_t n = 0;
  for (std::size_t i = 0; i < mcp9700_test::chip::kRowCount; ++i) {
    const auto& r = mcp9700_test::chip::kRows[i];
    out[n++] = {.config = mcp9700_test::NamedConfig(i),
                .v0 = r.v0_mv,
                .tc = r.tc_mv_per_c,
                .min_c = r.min_c,
                .max_c = r.max_c};
  }
  for (const auto& s : mcp9700_test::chip::kSynthetics) {
    out[n++] = {.config = mcp9700_test::CustomConfig(s),
                .v0 = static_cast<double>(s.v0_mv),
                .tc = static_cast<double>(s.tc_uv_per_c) / 1000.0,
                .min_c = static_cast<double>(s.min_mc) / 1000.0,
                .max_c = static_cast<double>(s.max_mc) / 1000.0};
  }
  return n;
}

void Sweep(const Part& p, uint16_t samples, double noise_mv) {
  MinimalPort port;
  // A converter that can see every part: 5 V reference, 12 bit.
  port.state.full_scale_mv = 5000;
  port.state.v0_mv = p.v0;
  port.state.tc_mv_per_c = p.tc;
  port.state.noise_mv = noise_mv;
  mcp9700::Config c = p.config;
  c.adc_full_scale_mv = 5000;
  c.samples = samples;
  c.max_spread_mv = 1000;
  Sensor sensor(port, c);
  const double tol_mc =
      static_cast<double>(sensor.ResolutionMilliC()) + (1000.0 * (0.5 + noise_mv) / p.tc) + 1.0;
  const double margin_c = tol_mc / 1000.0;
  const double lo_edge = p.min_c - 8.0;
  const double hi_edge = p.max_c + 8.0;
  port.state.temperature_mc = static_cast<int32_t>(((p.min_c + p.max_c) / 2.0) * 1000.0);
  REQUIRE(sensor.EnsureInitialized().ok());
  for (int t = -60; t <= 160; ++t) {
    const auto tc = static_cast<double>(t);
    port.state.temperature_mc = t * 1000;
    const double ideal_mv = p.v0 + (p.tc * tc);
    if (ideal_mv > 4900.0) {
      continue; // near the converter's top code: Saturated is the right answer
                // there
    }
    const auto m = sensor.ReadMeasurement();
    if ((tc >= (lo_edge + margin_c)) && (tc <= (hi_edge - margin_c)) && (ideal_mv > 2.0)) {
      REQUIRE(m.ok());
      CHECK(std::fabs(static_cast<double>(m.value.temperature_mc) - (tc * 1000.0)) <= tol_mc);
    } else if ((tc < (lo_edge - margin_c)) && ((p.v0 + (p.tc * (lo_edge - margin_c))) > 2.0)) {
      CHECK_EQ(m.error, Error::OutputLow);
    } else if (tc > (hi_edge + margin_c)) {
      CHECK_EQ(m.error, Error::OutputHigh);
    } else {
      // Within the rounding of an edge, or an output the part cannot make:
      // either answer is right.
    }
  }
}

} // namespace

TEST_CASE("Every named variant and every synthetic custom part tracks the "
          "model, -60 to +160 C") {
  Part parts[16];
  const std::size_t n = Parts(parts);
  CHECK_EQ(n, 10U);
  for (std::size_t i = 0; i < n; ++i) {
    for (const uint16_t samples : {uint16_t{1}, uint16_t{8}, uint16_t{64}}) {
      Sweep(parts[i], samples, 0.0);
      Sweep(parts[i], samples, 2.0);
    }
  }
}

TEST_CASE("The driver equals the oracle over random conversions of random "
          "valid configurations") {
  mcp9700_test::Rng rng;
  Part parts[16];
  const std::size_t n = Parts(parts);
  for (int round = 0; round < 3000; ++round) {
    mcp9700::Config c = parts[static_cast<std::size_t>(round) % n].config;
    c.samples = static_cast<uint16_t>(1 + static_cast<int>((rng.Next() + 1.0) * 31.0));
    c.discard_conversions = static_cast<uint8_t>(static_cast<int>((rng.Next() + 1.0) * 1.9));
    c.sample_interval_us = 0;
    c.divider_num = static_cast<uint16_t>(1 + static_cast<int>((rng.Next() + 1.0) * 3.0));
    c.divider_den = static_cast<uint16_t>(1 + static_cast<int>((rng.Next() + 1.0) * 2.0));
    c.adc_full_scale_mv = 3300 + static_cast<uint32_t>((rng.Next() + 1.0) * 1000.0);
    c.max_spread_mv = 1000;
    c.range_margin_mc = static_cast<int32_t>((rng.Next() + 1.0) * 8000.0);
    REQUIRE(mcp9700_ref::Valid(c));
    // The conversions are whatever the port says: a scripted port, not the
    // sensor model.
    struct Scripted : mcp9700::AdcInterface<Scripted> {
      uint32_t values[100]{};
      std::size_t next = 0;
      bool EnsureInitialized() noexcept {
        return true;
      }
      bool ReadVoltageMv(uint8_t /*channel*/, uint32_t& mv) noexcept {
        mv = values[next++];
        return true;
      }
      void DelayUs(uint32_t /*us*/) noexcept {}
    } port;
    const uint32_t centre =
        static_cast<uint32_t>((rng.Next() + 1.0) * 700.0 * static_cast<double>(c.divider_den) /
                              static_cast<double>(c.divider_num)) +
        1U;
    const std::size_t total = static_cast<std::size_t>(c.discard_conversions) + c.samples;
    for (uint32_t& v : port.values) {
      v = centre + static_cast<uint32_t>((rng.Next() + 1.0) * 5.0);
    }
    mcp9700::MCP9700<Scripted> sensor(port, c);
    // Start-up consumes `total` conversions; compare the first reading with the
    // oracle.
    const auto init = sensor.EnsureInitialized();
    const auto expect = mcp9700_ref::Evaluate(c, &port.values[c.discard_conversions], c.samples);
    if (expect.error == Error::Ok) {
      REQUIRE(init.ok());
    } else {
      CHECK_EQ(init.error, Error::DeviceIdMismatch);
    }
    port.next = 0;
    for (std::size_t i = 0; i < total; ++i) {
      port.values[i] = centre + static_cast<uint32_t>((rng.Next() + 1.0) * 5.0);
    }
    const auto expect2 = mcp9700_ref::Evaluate(c, &port.values[c.discard_conversions], c.samples);
    if (init.ok()) {
      const auto m = sensor.ReadMeasurement();
      CHECK_EQ(m.error, expect2.error);
      if (m.ok()) {
        CHECK_EQ(static_cast<int64_t>(m.value.temperature_mc) - c.offset_cal_mc,
                 expect2.temperature_mc);
        CHECK_EQ(static_cast<int64_t>(m.value.output_mv), expect2.output_mv);
        CHECK_EQ(static_cast<int64_t>(m.value.spread_mv), expect2.spread_mv);
      }
    }
  }
}

TEST_CASE("Accuracy offsets keep a part inside its datasheet band") {
  for (std::size_t i = 0; i < 5; ++i) {
    for (const bool upper : {false, true}) {
      MinimalPort port;
      port.state.UseRow(mcp9700_test::chip::kRows[i]);
      Sensor sensor(port, mcp9700_test::NamedConfig(i));
      const auto spec = sensor.GetSpec();
      REQUIRE(spec.ok());
      for (int32_t mc = spec.value.min_mc; mc <= spec.value.max_ext_mc; mc += 5000) {
        const auto lim =
            mcp9700::AccuracyLimitsFor(spec.value, mcp9700::TemperatureClass::Extended, mc);
        REQUIRE(lim.ok());
        port.state.temperature_mc = mc;
        port.state.accuracy_offset_mc = upper ? lim.value.max_mc : lim.value.min_mc;
        Sensor fresh(port, mcp9700_test::NamedConfig(i));
        port.state.temperature_mc = 25'000;
        port.state.accuracy_offset_mc = 0;
        REQUIRE(fresh.EnsureInitialized().ok());
        port.state.temperature_mc = mc;
        port.state.accuracy_offset_mc = upper ? lim.value.max_mc : lim.value.min_mc;
        const auto m = fresh.ReadMeasurement();
        REQUIRE(m.ok());
        const int32_t err = m.value.temperature_mc - mc;
        CHECK(err >= lim.value.min_mc - 200);
        CHECK(err <= lim.value.max_mc + 200);
      }
    }
  }
}

TEST_CASE("The nonlinearity (0.5 C at 0 and 70 C) stays inside the datasheet's "
          "typical band") {
  MinimalPort port;
  port.state.nonlinearity_c = 0.5;
  Sensor sensor(port);
  REQUIRE(sensor.EnsureInitialized().ok());
  for (const int32_t mc : {0, 25'000, 70'000}) {
    port.state.temperature_mc = mc;
    const auto m = sensor.ReadMeasurement();
    REQUIRE(m.ok());
    CHECK(std::abs(m.value.temperature_mc - mc) <= 600);
  }
}
