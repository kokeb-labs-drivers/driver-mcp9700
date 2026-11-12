/**
 * @file mcp9700_types.hpp
 * @brief Public types of the MCP970x driver: errors, `Result<T>`, the variant
 * table, configuration, presets, `Validate()` and the pure conversion
 * functions.
 * @copyright Copyright (c) 2026 The driver-mcp9700 authors.
 * SPDX-License-Identifier: MIT
 *
 * @details
 * The sensor's transfer function is first order, V_OUT = V_0C + T_C x T
 * (datasheet DS20001942L, Equation 4-1, p.10). Temperatures are milli-degrees
 * Celsius (`_mc`), voltages millivolts (`_mv`), coefficients microvolts per
 * degree (`_uv_per_c`); the driver uses integers only. One `constexpr` table
 * holds the five named parts; `Variant::Custom` takes a fully declared
 * first-order part.
 */
#ifndef MCP9700_TYPES_HPP
#define MCP9700_TYPES_HPP

#include <array>
#include <cstddef>
#include <cstdint>

#include "mcp9700_detail.hpp"
#include "mcp9700_version.h"

/** @brief The MCP970x driver: everything it declares lives here; internals in
 * `detail`. */
namespace mcp9700 {

/**
 * @defgroup mcp9700_types Driver types
 * @brief Errors, results, the variant table, configuration, presets and
 * conversions.
 * @{
 */

/**
 * @brief Why a call failed. Values 0 to 9 are the shared base set;
 * chip-specific values start at 16.
 *
 * An analog sensor has no bus protocol and no fault register: `Timeout`, `Crc`,
 * `Busy` and `DeviceFault` are never returned; its faults are `OutputLow`,
 * `OutputHigh`, `Saturated` and `Unstable`.
 */
enum class [[nodiscard]] Error : uint8_t {
  Ok = 0,               ///< The call succeeded.
  NotInitialized = 1,   ///< `EnsureInitialized()` has not succeeded, or
                        ///< `Deinitialize()`/`Sleep()` ran since.
  InvalidArgument = 2,  ///< `Validate()` failed (at `EnsureInitialized()`); nothing was sent.
  Bus = 3,              ///< The port's ADC bring-up, a conversion or a pin write failed.
  Timeout = 4,          ///< Not returned by this driver.
  DeviceIdMismatch = 5, ///< Start-up found no plausible, stable output on the channel.
  Crc = 6,              ///< Not returned by this driver.
  Busy = 7,             ///< Not returned by this driver.
  DeviceFault = 8,      ///< Not returned by this driver.
  Unsupported = 9,      ///< `Sleep()` needs the optional `GpioSet()` hook and the
                        ///< port has none.
  OutputLow = 16,       ///< The output is below the specified range minus the margin.
  OutputHigh = 17,      ///< The output is above the specified range plus the margin.
  Saturated = 18,       ///< A conversion is within one LSB of the declared full
                        ///< scale, or above it.
  Unstable = 19,        ///< The conversions of one reading differ by more than
                        ///< `max_spread_mv`.
};

/**
 * @brief The name of an error, for logs.
 * @param error The error.
 * @return A static string; `"Unknown"` for a value that is not an enumerator.
 */
constexpr const char* ToString(Error error) noexcept {
  switch (error) {
  case Error::Ok:
    return "Ok";
  case Error::NotInitialized:
    return "NotInitialized";
  case Error::InvalidArgument:
    return "InvalidArgument";
  case Error::Bus:
    return "Bus";
  case Error::Timeout:
    return "Timeout";
  case Error::DeviceIdMismatch:
    return "DeviceIdMismatch";
  case Error::Crc:
    return "Crc";
  case Error::Busy:
    return "Busy";
  case Error::DeviceFault:
    return "DeviceFault";
  case Error::Unsupported:
    return "Unsupported";
  case Error::OutputLow:
    return "OutputLow";
  case Error::OutputHigh:
    return "OutputHigh";
  case Error::Saturated:
    return "Saturated";
  case Error::Unstable:
    return "Unstable";
  default:
    break;
  }
  return "Unknown";
}
/**
 * @brief The value of a call, or why there is none.
 * @tparam T The value type. `Result<void>` carries only the error.
 */
template <typename T>
struct [[nodiscard]] Result {
  T value{};              ///< The value; `T{}` when the call failed.
  Error error{Error::Ok}; ///< `Error::Ok` on success.

  /** @return true when the call succeeded. */
  [[nodiscard]] constexpr bool ok() const noexcept {
    return error == Error::Ok;
  }
  /** @return true when the call succeeded. */
  constexpr explicit operator bool() const noexcept {
    return ok();
  }
  /**
   * @brief A successful result.
   * @param v The value.
   * @return `{v, Error::Ok}`.
   */
  static constexpr Result Success(T v) noexcept {
    return {v, Error::Ok};
  }
  /**
   * @brief A failed result.
   * @param e Why it failed; never `Error::Ok`.
   * @return `{T{}, e}`.
   */
  static constexpr Result Failure(Error e) noexcept {
    return {T{}, e};
  }
};

/** @brief The result of a call that returns no value. */
template <>
struct [[nodiscard]] Result<void> {
  Error error{Error::Ok}; ///< `Error::Ok` on success.

  /** @return true when the call succeeded. */
  [[nodiscard]] constexpr bool ok() const noexcept {
    return error == Error::Ok;
  }
  /** @return true when the call succeeded. */
  constexpr explicit operator bool() const noexcept {
    return ok();
  }
  /** @return A successful result. */
  static constexpr Result Success() noexcept {
    return {Error::Ok};
  }
  /**
   * @brief A failed result.
   * @param e Why it failed; never `Error::Ok`.
   * @return `{e}`.
   */
  static constexpr Result Failure(Error e) noexcept {
    return {e};
  }
};

/**
 * @brief Which part of the family is wired up: one of the five datasheet
 * devices, or `Custom`.
 *
 * The parts cannot be told apart at run time (they differ in accuracy grade,
 * not in transfer function, except the MCP9701 family), so this is a
 * declaration by the user, never detected.
 */
enum class Variant : uint8_t {
  Mcp9700 = 0,  ///< 500 mV at 0 C, 10.0 mV/C, -40 to +125 C (High: +150 C),
                ///< accuracy +-4 C.
  Mcp9700A = 1, ///< As the MCP9700 with the +-2 C grade (0 to +70 C).
  Mcp9700B = 2, ///< As the MCP9700 with the +-1 C grade (+20 to +70 C); High
                ///< option to +150 C.
  Mcp9701 = 3,  ///< 400 mV at 0 C, 19.5 mV/C, -10 to +125 C, accuracy +-4 C.
  Mcp9701A = 4, ///< As the MCP9701 with the +-2 C grade (0 to +70 C).
  Custom = 5,   ///< Another first-order linear sensor, fully declared in
                ///< `Config::custom`.
};

/** @brief The temperature class of the part: the upper end of its specified
 * range. */
enum class TemperatureClass : uint8_t {
  Extended = 0, ///< -40 (MCP9701: -10) to +125 C, every part.
  High = 1,     ///< -40 to +150 C: ordering code H, MCP9700 and MCP9700B in
                ///< SOT-23-3 and SC70-5 only.
};

namespace detail {
/// Last enumerator of `Variant` (`IsEnumerator`).
template <>
struct EnumLast<Variant> {
  static constexpr Variant kValue = Variant::Custom; ///< The last enumerator.
};
/// Last enumerator of `TemperatureClass`.
template <>
struct EnumLast<TemperatureClass> {
  static constexpr TemperatureClass kValue = TemperatureClass::High; ///< The last enumerator.
};
} // namespace detail

/** @brief Largest number of conversions one reading averages. */
inline constexpr uint16_t kMaxSamples = 64U;
/** @brief Largest wait between two conversions, microseconds. */
inline constexpr uint32_t kMaxSampleIntervalUs = 100'000U;
/** @brief Largest number of dummy conversions before a reading. */
inline constexpr uint8_t kMaxDiscard = 4U;
/** @brief Largest power-on wait, microseconds. */
inline constexpr uint32_t kMaxPowerOnSettleUs = 1'000'000U;
/**
 * @brief Longest span of one reading, `(discard + samples - 1) x interval`,
 * microseconds: below the 1.3 s thermal response (t_RES, p.4), so an average is
 * never staler than the sensor (design decision).
 */
inline constexpr uint64_t kMaxAveragingWindowUs = 1'000'000ULL;
/** @brief Largest `max_spread_mv`. */
inline constexpr uint32_t kMaxSpreadLimitMv = 1000U;
/** @brief Largest absolute `offset_cal_mc`, milli-degrees. */
inline constexpr int32_t kMaxOffsetCalMc = 20'000;
/** @brief Largest `range_margin_mc`, milli-degrees. */
inline constexpr int32_t kMaxRangeMarginMc = 20'000;
/** @brief Smallest declarable ADC full scale, millivolts. */
inline constexpr uint32_t kMinAdcFullScaleMv = 100U;
/** @brief Largest declarable ADC full scale, millivolts. */
inline constexpr uint32_t kMaxAdcFullScaleMv = 10'000U;
/** @brief Smallest declarable ADC resolution, bits. */
inline constexpr uint8_t kMinAdcResolutionBits = 8U;
/** @brief Largest declarable ADC resolution, bits. */
inline constexpr uint8_t kMaxAdcResolutionBits = 16U;
/** @brief `Custom`: smallest slope, microvolts per degree. */
inline constexpr uint32_t kCustomMinTcUvPerC = 1000U;
/** @brief `Custom`: largest slope, microvolts per degree. */
inline constexpr uint32_t kCustomMaxTcUvPerC = 100'000U;
/** @brief `Custom`: largest output at 0 C, millivolts. */
inline constexpr uint32_t kCustomMaxV0Mv = 5000U;
/** @brief `Custom`: largest absolute temperature limit, milli-degrees. */
inline constexpr int32_t kCustomMaxTemperatureMc = 200'000;
/** @brief `Custom`: largest output voltage anywhere in the declared range,
 * millivolts. */
inline constexpr uint32_t kCustomMaxOutputMv = 20'000U;
/** @brief `Custom`: largest absolute accuracy limit, milli-degrees. */
inline constexpr int32_t kCustomMaxAccuracyMc = 20'000;

/**
 * @brief What the user declares for `Variant::Custom`. Every field must be set
 * (an all-zero declaration fails `Validate()`); every field must stay zero for
 * a named variant.
 *
 * The driver cannot check that these numbers describe the part: a wrong value
 * gives wrong temperatures that look plausible.
 */
struct CustomSpec {
  uint32_t v0_mv{0U};         ///< Output at 0 C, millivolts (0 is valid).
  uint32_t tc_uv_per_c{0U};   ///< Slope, microvolts per C, 1000 to 100000 (positive only).
  int32_t min_mc{0};          ///< Lowest specified temperature, milli-C.
  int32_t max_mc{0};          ///< Highest specified temperature, milli-C.
  int32_t accuracy_min_mc{0}; ///< Worst-case error over [min, max], lower
                              ///< limit, milli-C (at most 0).
  int32_t accuracy_max_mc{0}; ///< Worst-case error over [min, max], upper
                              ///< limit, milli-C (at least 0).
};

/** @brief Everything `EnsureInitialized()` uses. `Config{}` is an MCP9700 on a
 * 12 bit, 3.3 V ADC.
 */
struct Config {
  uint8_t channel{0U};                                ///< Passed to `ReadVoltageMv()` unchanged.
  Variant variant{Variant::Mcp9700};                  ///< A named part, or `Custom`.
  TemperatureClass range{TemperatureClass::Extended}; ///< `High` only for a part with that option.
  CustomSpec custom{};                                ///< Only with `Variant::Custom`.
  uint32_t adc_full_scale_mv{3300U};                  ///< Largest voltage the port can report, mV.
  uint8_t adc_resolution_bits{12U};                   ///< ADC resolution, 8 to 16 bits.
  uint16_t divider_num{1U};                           ///< VOUT = pin x divider_num / divider_den.
  uint16_t divider_den{1U};                           ///< See `divider_num`.
  int32_t offset_cal_mc{0};          ///< Added to the result: one-point calibration.
  int32_t range_margin_mc{8000};     ///< Plausibility margin each side of the range, milli-C.
  uint16_t samples{8U};              ///< Conversions averaged per reading, 1 to `kMaxSamples`.
  uint32_t sample_interval_us{100U}; ///< Wait between conversions, up to `kMaxSampleIntervalUs`.
  uint8_t discard_conversions{1U};   ///< Dummy conversions before a reading, up to `kMaxDiscard`.
  uint32_t max_spread_mv{30U};       ///< Largest max-min of one reading at VOUT, mV.
  uint32_t power_on_settle_us{
      2400U}; ///< Wait after enabling the sensor, up to `kMaxPowerOnSettleUs`.
};

/** @brief One accuracy limit of the datasheet: the error band guaranteed over a
 * temperature span.
 */
struct AccuracyBand {
  int32_t from_mc{0};    ///< Lower end of the span, milli-C.
  int32_t to_mc{0};      ///< Upper end of the span, milli-C.
  int32_t min_mc{0};     ///< Lowest error, milli-C.
  int32_t max_mc{0};     ///< Highest error, milli-C.
  bool high_only{false}; ///< The row exists for the High temperature class only.
};

/** @brief Package bit: SOT-23-3 (`/TT`). */
inline constexpr uint8_t kPackageSot23 = 1U;
/** @brief Package bit: SC70-5 (`/LT`). */
inline constexpr uint8_t kPackageSc70 = 2U;
/** @brief Package bit: TO-92-3 (`/TO`). */
inline constexpr uint8_t kPackageTo92 = 4U;

/** @brief Every package: SOT-23-3, SC70-5 and TO-92-3. */
inline constexpr uint8_t kPackagesAll =
    static_cast<uint8_t>(kPackageSot23 + kPackageSc70 + kPackageTo92);
/** @brief The packages without TO-92-3 (the MCP9700B, and the High option of
 * every part). */
inline constexpr uint8_t kPackagesNoTo92 = static_cast<uint8_t>(kPackageSot23 + kPackageSc70);

/** @brief Most accuracy bands one row holds. */
inline constexpr std::size_t kMaxBands = 4U;

/**
 * @brief One row of the variant table: the only place a coefficient, range,
 * supply, accuracy band or package list lives. The code never branches on a
 * variant name.
 */
struct VariantSpec {
  uint32_t v0_mv{0U};         ///< Output at 0 C, millivolts (typical).
  uint32_t tc_uv_per_c{0U};   ///< Slope, microvolts per C (typical).
  int32_t min_mc{0};          ///< Lowest specified temperature, milli-C.
  int32_t max_ext_mc{0};      ///< Highest, Extended class, milli-C.
  int32_t max_high_mc{0};     ///< Highest, High class, milli-C (= Extended when none).
  bool has_high{false};       ///< The row has a High class.
  uint32_t vdd_min_mv{0U};    ///< Lowest supply, mV (0 when unknown).
  uint32_t vdd_max_mv{0U};    ///< Highest supply, mV (0 when unknown).
  uint32_t tested_vdd_mv{0U}; ///< Supply at which the accuracy is tested, mV (0 unknown).
  uint8_t packages{0U};       ///< `kPackage*` mask of the packages sold; 0 for `Custom`.
  uint8_t high_packages{0U};  ///< `kPackage*` mask of the High option.
  uint8_t band_count{0U};     ///< Used entries of `bands`.
  std::array<AccuracyBand, kMaxBands> bands{}; ///< Accuracy bands, narrowest first.
};

/** @brief The guaranteed error limits at one temperature, milli-C. */
struct AccuracyLimits {
  int32_t min_mc{0}; ///< Lowest error.
  int32_t max_mc{0}; ///< Highest error.
};

namespace detail {
/// One accuracy band of the table, with named fields.
constexpr AccuracyBand Band(int32_t from_mc, int32_t to_mc, int32_t min_mc, int32_t max_mc,
                            bool high_only) noexcept {
  return {.from_mc = from_mc,
          .to_mc = to_mc,
          .min_mc = min_mc,
          .max_mc = max_mc,
          .high_only = high_only};
}
} // namespace detail

/**
 * @brief The five datasheet devices (DS20001942L), indexed by `Variant`. Every
 * value is cited in `docs/hardware.md`: coefficients p.3 and p.4, supply p.3,
 * ranges p.4, accuracy p.3, packages p.12 and p.23.
 */
inline constexpr std::array<VariantSpec, 5> kVariantTable{
    {// MCP9700
     {.v0_mv = 500U,
      .tc_uv_per_c = 10'000U,
      .min_mc = -40'000,
      .max_ext_mc = 125'000,
      .max_high_mc = 150'000,
      .has_high = true,
      .vdd_min_mv = 2300U,
      .vdd_max_mv = 5500U,
      .tested_vdd_mv = 3300U,
      .packages = kPackagesAll,
      .high_packages = kPackagesNoTo92,
      .band_count = 3U,
      .bands = {{detail::Band(0, 70'000, -4000, 4000, false),
                 detail::Band(-40'000, 125'000, -4000, 6000, false),
                 detail::Band(-40'000, 150'000, -4000, 6000, true)}}},
     // MCP9700A
     {.v0_mv = 500U,
      .tc_uv_per_c = 10'000U,
      .min_mc = -40'000,
      .max_ext_mc = 125'000,
      .max_high_mc = 125'000,
      .has_high = false,
      .vdd_min_mv = 2300U,
      .vdd_max_mv = 5500U,
      .tested_vdd_mv = 3300U,
      .packages = kPackagesAll,
      .high_packages = 0U,
      .band_count = 2U,
      .bands = {{detail::Band(0, 70'000, -2000, 2000, false),
                 detail::Band(-40'000, 125'000, -2000, 4000, false)}}},
     // MCP9700B
     {.v0_mv = 500U,
      .tc_uv_per_c = 10'000U,
      .min_mc = -40'000,
      .max_ext_mc = 125'000,
      .max_high_mc = 150'000,
      .has_high = true,
      .vdd_min_mv = 2300U,
      .vdd_max_mv = 5500U,
      .tested_vdd_mv = 3300U,
      .packages = kPackagesNoTo92,
      .high_packages = kPackagesNoTo92,
      .band_count = 4U,
      .bands = {{detail::Band(20'000, 70'000, -1000, 1000, false),
                 detail::Band(0, 125'000, -2000, 3000, false),
                 detail::Band(-40'000, 125'000, -2000, 4000, false),
                 detail::Band(-40'000, 150'000, -4000, 4000, true)}}},
     // MCP9701
     {.v0_mv = 400U,
      .tc_uv_per_c = 19'500U,
      .min_mc = -10'000,
      .max_ext_mc = 125'000,
      .max_high_mc = 125'000,
      .has_high = false,
      .vdd_min_mv = 3100U,
      .vdd_max_mv = 5500U,
      .tested_vdd_mv = 5000U,
      .packages = kPackagesAll,
      .high_packages = 0U,
      .band_count = 2U,
      .bands = {{detail::Band(0, 70'000, -4000, 4000, false),
                 detail::Band(-10'000, 125'000, -4000, 6000, false)}}},
     // MCP9701A
     {.v0_mv = 400U,
      .tc_uv_per_c = 19'500U,
      .min_mc = -10'000,
      .max_ext_mc = 125'000,
      .max_high_mc = 125'000,
      .has_high = false,
      .vdd_min_mv = 3100U,
      .vdd_max_mv = 5500U,
      .tested_vdd_mv = 5000U,
      .packages = kPackagesAll,
      .high_packages = 0U,
      .band_count = 2U,
      .bands = {{detail::Band(0, 70'000, -2000, 2000, false),
                 detail::Band(-10'000, 125'000, -2000, 4000, false)}}}}};

/** @brief The temperature and the voltage behind it, from one validated
 * reading. */
struct Measurement {
  int32_t temperature_mc{0}; ///< Temperature after `offset_cal_mc`, milli-C.
  uint32_t output_mv{0U};    ///< Mean VOUT, scaled by the divider, millivolts.
  uint32_t spread_mv{0U};    ///< Max minus min of the conversions, scaled, millivolts.
};

/** @brief Severity passed to the optional `Log()` hook. */
enum class LogLevel : uint8_t {
  Error = 0, ///< A call failed.
  Warning,   ///< Something unexpected the driver continued from.
  Info,      ///< Lifecycle events.
  Debug,     ///< Detail for bring-up.
};

namespace detail {

/// Nanovolts per millivolt and microvolts per volt: the one named multiplier of the integer
/// arithmetic.
inline constexpr int64_t kPerMega = 1'000'000; ///< Nanovolts per millivolt, microvolts per volt.

/// `num / den` rounded half away from zero; `den` is positive.
constexpr int64_t RoundDivAway(int64_t num, int64_t den) noexcept {
  const int64_t half = den / 2;
  return (num >= 0) ? ((num + half) / den) : (-((-num + half) / den));
}

/// Nanovolts at `t_mc` for a first-order part: V0 x 1e6 + TC x T.
constexpr int64_t OutputNanoVolts(uint32_t v0_mv, uint32_t tc_uv_per_c, int64_t t_mc) noexcept {
  return (static_cast<int64_t>(v0_mv) * kPerMega) + (static_cast<int64_t>(tc_uv_per_c) * t_mc);
}

/// `x` within `[-limit, limit]`.
constexpr bool WithinSymmetric(int32_t x, int32_t limit) noexcept {
  return (x >= -limit) && (x <= limit);
}

/// A named variant's `custom` field set that is not all zero.
constexpr bool CustomIsZero(const CustomSpec& c) noexcept {
  return (c.v0_mv == 0U) && (c.tc_uv_per_c == 0U) && (c.min_mc == 0) && (c.max_mc == 0) &&
         (c.accuracy_min_mc == 0) && (c.accuracy_max_mc == 0);
}

/// The output of a `Custom` at `t_mc` lies in the window an ADC can see (0 to
/// 20000 mV).
constexpr bool CustomOutputInWindow(const CustomSpec& c, int32_t t_mc) noexcept {
  const int64_t nv = OutputNanoVolts(c.v0_mv, c.tc_uv_per_c, t_mc);
  return (nv >= 0) && (nv <= (static_cast<int64_t>(kCustomMaxOutputMv) * kPerMega));
}

/// The transfer function and range of a `Custom` declaration.
constexpr bool CustomTransferOk(const CustomSpec& c) noexcept {
  return (c.tc_uv_per_c >= kCustomMinTcUvPerC) && (c.tc_uv_per_c <= kCustomMaxTcUvPerC) &&
         (c.v0_mv <= kCustomMaxV0Mv) && (c.min_mc < c.max_mc) &&
         WithinSymmetric(c.min_mc, kCustomMaxTemperatureMc) &&
         WithinSymmetric(c.max_mc, kCustomMaxTemperatureMc) && CustomOutputInWindow(c, c.min_mc) &&
         CustomOutputInWindow(c, c.max_mc);
}

/// The accuracy declaration of a `Custom`: it brackets zero and is not "no
/// error at all".
constexpr bool CustomAccuracyOk(const CustomSpec& c) noexcept {
  return (c.accuracy_min_mc <= 0) && (c.accuracy_max_mc >= 0) &&
         ((c.accuracy_min_mc != 0) || (c.accuracy_max_mc != 0)) &&
         WithinSymmetric(c.accuracy_min_mc, kCustomMaxAccuracyMc) &&
         WithinSymmetric(c.accuracy_max_mc, kCustomMaxAccuracyMc);
}

/// Variant, range and the `custom` declaration agree (a named part carries
/// none; a `Custom` all).
constexpr bool VariantDeclarationOk(const Config& config) noexcept {
  if (!IsEnumerator(config.variant) || !IsEnumerator(config.range)) {
    return false;
  }
  if (config.variant == Variant::Custom) {
    return (config.range == TemperatureClass::Extended) && CustomTransferOk(config.custom) &&
           CustomAccuracyOk(config.custom);
  }
  if (!CustomIsZero(config.custom)) {
    return false;
  }
  return (config.range != TemperatureClass::High) ||
         kVariantTable[static_cast<std::size_t>(ToUnderlying(config.variant))].has_high;
}

/// The ADC declaration, divider, calibration and margin.
constexpr bool AdcDeclarationOk(const Config& config) noexcept {
  return (config.adc_full_scale_mv >= kMinAdcFullScaleMv) &&
         (config.adc_full_scale_mv <= kMaxAdcFullScaleMv) &&
         (config.adc_resolution_bits >= kMinAdcResolutionBits) &&
         (config.adc_resolution_bits <= kMaxAdcResolutionBits) && (config.divider_num != 0U) &&
         (config.divider_den != 0U) && WithinSymmetric(config.offset_cal_mc, kMaxOffsetCalMc) &&
         (config.range_margin_mc >= 0) && (config.range_margin_mc <= kMaxRangeMarginMc);
}

/// Sampling counts and waits; the averaging window is computed in 64 bits.
constexpr bool SamplingOk(const Config& config) noexcept {
  if ((config.samples == 0U) || (config.samples > kMaxSamples) ||
      (config.discard_conversions > kMaxDiscard) ||
      (config.sample_interval_us > kMaxSampleIntervalUs)) {
    return false;
  }
  const uint64_t conversions = static_cast<uint64_t>(config.discard_conversions) +
                               static_cast<uint64_t>(config.samples) - 1U;
  return ((conversions * static_cast<uint64_t>(config.sample_interval_us)) <=
          kMaxAveragingWindowUs) &&
         (config.max_spread_mv <= kMaxSpreadLimitMv) &&
         (config.power_on_settle_us <= kMaxPowerOnSettleUs);
}

} // namespace detail

/**
 * @brief Check a configuration without touching the hardware.
 * @param config The configuration.
 * @return `Error::Ok` or `Error::InvalidArgument`: a variant or range that is
 * no enumerator, a named variant with a non-zero `custom` field, an incomplete
 * or inconsistent `Custom` declaration, `range = High` where the part has none,
 * or any field outside its documented range.
 */
constexpr Error Validate(const Config& config) noexcept {
  if (!detail::VariantDeclarationOk(config) || !detail::AdcDeclarationOk(config) ||
      !detail::SamplingOk(config)) {
    return Error::InvalidArgument;
  }
  return Error::Ok;
}

/**
 * @brief The effective variant row of a configuration: a table row, or the row
 * built from the `Custom` declaration. No I/O.
 * @param config The configuration.
 * @return The row, or `InvalidArgument` when `Validate(config)` fails.
 */
constexpr Result<VariantSpec> ResolveSpec(const Config& config) noexcept {
  if (Validate(config) != Error::Ok) {
    return Result<VariantSpec>::Failure(Error::InvalidArgument);
  }
  if (config.variant != Variant::Custom) {
    return Result<VariantSpec>::Success(
        kVariantTable[static_cast<std::size_t>(detail::ToUnderlying(config.variant))]);
  }
  VariantSpec spec{};
  spec.v0_mv = config.custom.v0_mv;
  spec.tc_uv_per_c = config.custom.tc_uv_per_c;
  spec.min_mc = config.custom.min_mc;
  spec.max_ext_mc = config.custom.max_mc;
  spec.max_high_mc = config.custom.max_mc;
  spec.band_count = 1U;
  spec.bands[0] = detail::Band(config.custom.min_mc, config.custom.max_mc,
                               config.custom.accuracy_min_mc, config.custom.accuracy_max_mc, false);
  return Result<VariantSpec>::Success(spec);
}

/**
 * @brief The temperature of an output voltage, from the row's V0 and TC;
 * rounded half away from zero. No window check, no calibration.
 * @param spec The variant row.
 * @param output_mv Output voltage in millivolts; values above 65535 are treated
 * as 65535.
 * @return Milli-degrees Celsius.
 */
constexpr int32_t OutputMvToMilliC(const VariantSpec& spec, uint32_t output_mv) noexcept {
  constexpr uint32_t kLargest = 65'535U;
  const uint32_t mv = (output_mv < kLargest) ? output_mv : kLargest;
  const int64_t num =
      (static_cast<int64_t>(mv) - static_cast<int64_t>(spec.v0_mv)) * detail::kPerMega;
  return static_cast<int32_t>(detail::RoundDivAway(num, static_cast<int64_t>(spec.tc_uv_per_c)));
}

/**
 * @brief The output voltage of a temperature: the inverse of
 * `OutputMvToMilliC()`, rounded to the nearest millivolt.
 * @param spec The variant row.
 * @param temperature_mc Temperature in milli-degrees Celsius.
 * @return Millivolts; 0 for a temperature whose output would be negative.
 */
constexpr uint32_t MilliCToOutputMv(const VariantSpec& spec, int32_t temperature_mc) noexcept {
  const int64_t nv = detail::OutputNanoVolts(spec.v0_mv, spec.tc_uv_per_c, temperature_mc);
  if (nv <= 0) {
    return 0U;
  }
  const int64_t mv = detail::RoundDivAway(nv, detail::kPerMega);
  constexpr int64_t kLargest = 0xFFFF'FFFFLL;
  return static_cast<uint32_t>((mv < kLargest) ? mv : kLargest);
}

/**
 * @brief The error limits the datasheet guarantees at a temperature (first band
 * that contains it, narrowest first). With `High` only the High-option rows
 * (-40 to +150 C) apply over the whole range: the datasheet does not say that an
 * H part also meets the tighter Extended rows. For a named variant they hold at its
 * tested supply; the driver never applies them to a reading.
 * @param spec The variant row.
 * @param range The temperature class.
 * @param temperature_mc Temperature in milli-degrees Celsius.
 * @return The limits, or `InvalidArgument` outside the specified range (or
 * `High` on a row without that class).
 */
constexpr Result<AccuracyLimits> AccuracyLimitsFor(const VariantSpec& spec, TemperatureClass range,
                                                   int32_t temperature_mc) noexcept {
  const bool high = range == TemperatureClass::High;
  const int32_t top = high ? spec.max_high_mc : spec.max_ext_mc;
  if ((high && !spec.has_high) || (temperature_mc < spec.min_mc) || (temperature_mc > top)) {
    return Result<AccuracyLimits>::Failure(Error::InvalidArgument);
  }
  for (std::size_t i = 0; i < kMaxBands; ++i) {
    const AccuracyBand& band = spec.bands[i];
    if ((i < static_cast<std::size_t>(spec.band_count)) && (band.high_only == high) &&
        (temperature_mc >= band.from_mc) && (temperature_mc <= band.to_mc)) {
      return Result<AccuracyLimits>::Success({.min_mc = band.min_mc, .max_mc = band.max_mc});
    }
  }
  return Result<AccuracyLimits>::Failure(Error::InvalidArgument);
}

/**
 * @brief A temperature as `float` degrees Celsius, for printing and for callers
 * that want it. The only floating-point in the driver.
 * @param milli_c Milli-degrees Celsius.
 * @return Degrees Celsius.
 */
constexpr float ToCelsius(int32_t milli_c) noexcept {
  return static_cast<float>(milli_c) / 1000.0F;
}

/**
 * @brief The configuration of a named part: `Config{}` with that variant.
 * @param variant One of the five named variants (and `Custom`, which still
 * needs its declaration).
 * @return The configuration.
 */
constexpr Config ConfigFor(Variant variant) noexcept {
  Config c{};
  c.variant = variant;
  return c;
}

/**
 * @brief One conversion per reading: lowest latency, no noise rejection.
 * @return The preset (an MCP9700).
 */
constexpr Config FastPreset() noexcept {
  Config c{};
  c.samples = 1U;
  c.sample_interval_us = 0U;
  return c;
}

/**
 * @brief 32 conversions 200 us apart: averages ADC noise and mains pickup.
 * @return The preset (an MCP9700).
 */
constexpr Config LowNoisePreset() noexcept {
  Config c{};
  c.samples = 32U;
  c.sample_interval_us = 200U;
  return c;
}

/**
 * @brief The MCP9701 transfer function and range.
 * @return The preset.
 */
constexpr Config Mcp9701Preset() noexcept {
  return ConfigFor(Variant::Mcp9701);
}

/**
 * @brief An MCP9700 with the High temperature option (to +150 C).
 * @return The preset.
 */
constexpr Config HighTemperaturePreset() noexcept {
  Config c{};
  c.range = TemperatureClass::High;
  return c;
}

static_assert(Validate(Config{}) == Error::Ok, "the default configuration is valid");
static_assert(Validate(FastPreset()) == Error::Ok, "FastPreset is valid");
static_assert(Validate(LowNoisePreset()) == Error::Ok, "LowNoisePreset is valid");
static_assert(Validate(Mcp9701Preset()) == Error::Ok, "Mcp9701Preset is valid");
static_assert(Validate(HighTemperaturePreset()) == Error::Ok, "HighTemperaturePreset is valid");

/** @return The driver version, e.g. `"0.1.0"`. No I/O. */
constexpr const char* GetDriverVersion() noexcept {
  return MCP9700_VERSION_STRING;
}
/** @return The major version. No I/O. */
constexpr uint32_t GetDriverVersionMajor() noexcept {
  return MCP9700_VERSION_MAJOR;
}
/** @return The minor version. No I/O. */
constexpr uint32_t GetDriverVersionMinor() noexcept {
  return MCP9700_VERSION_MINOR;
}
/** @return The patch version. No I/O. */
constexpr uint32_t GetDriverVersionPatch() noexcept {
  return MCP9700_VERSION_PATCH;
}

/** @} */

} // namespace mcp9700

#endif // MCP9700_TYPES_HPP
