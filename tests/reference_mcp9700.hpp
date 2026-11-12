/**
 * @file reference_mcp9700.hpp
 * @brief A second, deliberately plain implementation of the driver's rules,
 * used as the oracle of the differential tests and the fuzz target.
 *
 * It keeps its own copy of the datasheet rows and of the `Custom` rules, rounds
 * each division from its exact quotient and remainder, and shares no code with
 * the driver (only the `Config` and `Error` types), so an error in the driver's
 * integer maths or table cannot hide behind the same error here.
 */
#ifndef MCP9700_REFERENCE_MCP9700_HPP
#define MCP9700_REFERENCE_MCP9700_HPP

#include <cstddef>
#include <cstdint>

#include "mcp9700_types.hpp"

namespace mcp9700_ref {

/// The integer of the oracle: every intermediate fits it (see
/// `RoundDivScaled()` for the one product that would not).
using Wide = int64_t;

/// One datasheet row in integer units (DS20001942L p.3, p.4).
struct Row {
  int64_t v0_mv;
  int64_t tc_uv;
  int64_t min_mc;
  int64_t max_mc;
  int64_t max_high_mc;
  bool has_high;
};

inline constexpr Row kRows[5] = {
    {500, 10000, -40000, 125000, 150000, true},  {500, 10000, -40000, 125000, 125000, false},
    {500, 10000, -40000, 125000, 150000, true},  {400, 19500, -10000, 125000, 125000, false},
    {400, 19500, -10000, 125000, 125000, false},
};

/// Round a/b half away from zero (b > 0), exactly.
inline Wide RoundDiv(Wide a, Wide b) {
  if (b <= 0) {
    return 0;
  }
  const Wide mag = (a < 0) ? -a : a;
  const Wide q = ((2 * mag) + b) / (2 * b);
  return (a < 0) ? -q : q;
}

/// Round n x 1000000 / d half away from zero (d > 0) from the quotient and the
/// remainder of n / d, so that no intermediate exceeds 64 bits. A result beyond
/// 10^16 in magnitude saturates there: far outside any window.
inline Wide RoundDivScaled(Wide n, Wide d) {
  constexpr Wide kScale = 1'000'000;
  constexpr Wide kWholeCap = 10'000'000'000;
  if (d <= 0) {
    return 0;
  }
  const Wide mag = (n < 0) ? -n : n;
  Wide whole = mag / d;
  const Wide rem = mag % d;
  whole = (whole > kWholeCap) ? kWholeCap : whole;
  const Wide frac = ((2 * rem * kScale) + d) / (2 * d);
  const Wide q = (whole * kScale) + frac;
  return (n < 0) ? -q : q;
}

/// The `Custom` and field rules of the design, written a second time. True when
/// `c` is valid.
inline bool Valid(const mcp9700::Config& c) {
  const int variant = static_cast<int>(c.variant);
  const int range = static_cast<int>(c.range);
  if ((variant < 0) || (variant > 5) || (range < 0) || (range > 1)) {
    return false;
  }
  const auto& u = c.custom;
  const bool zero = (u.v0_mv == 0) && (u.tc_uv_per_c == 0) && (u.min_mc == 0) && (u.max_mc == 0) &&
                    (u.accuracy_min_mc == 0) && (u.accuracy_max_mc == 0);
  if (variant == 5) {
    if (range != 0) {
      return false;
    }
    const int64_t lo = (static_cast<int64_t>(u.v0_mv) * 1'000'000) +
                       (static_cast<int64_t>(u.tc_uv_per_c) * u.min_mc);
    const int64_t hi = (static_cast<int64_t>(u.v0_mv) * 1'000'000) +
                       (static_cast<int64_t>(u.tc_uv_per_c) * u.max_mc);
    const int64_t cap = 20'000LL * 1'000'000;
    const bool ok = (u.tc_uv_per_c >= 1000) && (u.tc_uv_per_c <= 100'000) && (u.v0_mv <= 5000) &&
                    (u.min_mc < u.max_mc) && (u.min_mc >= -200'000) && (u.max_mc <= 200'000) &&
                    (u.min_mc <= 200'000) && (u.max_mc >= -200'000) && (lo >= 0) && (lo <= cap) &&
                    (hi >= 0) && (hi <= cap) && (u.accuracy_min_mc <= 0) &&
                    (u.accuracy_max_mc >= 0) && (u.accuracy_min_mc >= -20'000) &&
                    (u.accuracy_max_mc <= 20'000) &&
                    ((u.accuracy_min_mc != 0) || (u.accuracy_max_mc != 0));
    if (!ok) {
      return false;
    }
  } else {
    if (!zero) {
      return false;
    }
    if ((range == 1) && !kRows[variant].has_high) {
      return false;
    }
  }
  if ((c.adc_full_scale_mv < 100) || (c.adc_full_scale_mv > 10'000) ||
      (c.adc_resolution_bits < 8) || (c.adc_resolution_bits > 16) || (c.divider_num == 0) ||
      (c.divider_den == 0) || (c.offset_cal_mc < -20'000) || (c.offset_cal_mc > 20'000) ||
      (c.range_margin_mc < 0) || (c.range_margin_mc > 20'000) || (c.samples == 0) ||
      (c.samples > 64) || (c.sample_interval_us > 100'000) || (c.discard_conversions > 4) ||
      (c.max_spread_mv > 1000) || (c.power_on_settle_us > 1'000'000)) {
    return false;
  }
  const uint64_t window = (static_cast<uint64_t>(c.discard_conversions) + c.samples - 1U) *
                          static_cast<uint64_t>(c.sample_interval_us);
  return window <= 1'000'000U;
}

/// The row of a valid configuration, as integers.
inline Row RowOf(const mcp9700::Config& c) {
  if (c.variant == mcp9700::Variant::Custom) {
    return {c.custom.v0_mv,  c.custom.tc_uv_per_c, c.custom.min_mc,
            c.custom.max_mc, c.custom.max_mc,      false};
  }
  return kRows[static_cast<int>(c.variant)];
}

struct Result {
  mcp9700::Error error;
  int64_t temperature_mc; ///< Before the calibration.
  int64_t output_mv;
  int64_t spread_mv;
};

/// The reading checks of a valid configuration over the conversions `mv[0..n)`
/// (the real ones).
inline Result Evaluate(const mcp9700::Config& c, const uint32_t* mv, std::size_t n) {
  const Row r = RowOf(c);
  Wide sum = 0;
  uint32_t lo = mv[0];
  uint32_t hi = mv[0];
  for (std::size_t i = 0; i < n; ++i) {
    sum += mv[i];
    lo = (mv[i] < lo) ? mv[i] : lo;
    hi = (mv[i] > hi) ? mv[i] : hi;
  }
  const Wide codes = static_cast<Wide>(uint64_t{1} << c.adc_resolution_bits);
  Wide lsb = (Wide{c.adc_full_scale_mv} + codes - 1) / codes;
  lsb = (lsb < 1) ? 1 : lsb;
  if ((Wide{hi} + lsb) >= Wide{c.adc_full_scale_mv}) {
    return {mcp9700::Error::Saturated, 0, 0, 0};
  }
  const Wide num = c.divider_num;
  const Wide den = c.divider_den;
  const Wide spread = RoundDiv(Wide{hi - lo} * num, den);
  if (spread > Wide{c.max_spread_mv}) {
    return {mcp9700::Error::Unstable, 0, 0, 0};
  }
  const Wide count = static_cast<Wide>(n);
  const Wide out = RoundDiv(sum * num, count * den);
  const Wide raw = RoundDivScaled((sum * num) - (count * den * r.v0_mv), count * den * r.tc_uv);
  const int64_t top = (c.range == mcp9700::TemperatureClass::High) ? r.max_high_mc : r.max_mc;
  if (raw < Wide{r.min_mc - c.range_margin_mc}) {
    return {mcp9700::Error::OutputLow, 0, 0, 0};
  }
  if (raw > Wide{top + c.range_margin_mc}) {
    return {mcp9700::Error::OutputHigh, 0, 0, 0};
  }
  return {mcp9700::Error::Ok, static_cast<int64_t>(raw), static_cast<int64_t>(out),
          static_cast<int64_t>(spread)};
}

} // namespace mcp9700_ref

#endif // MCP9700_REFERENCE_MCP9700_HPP
