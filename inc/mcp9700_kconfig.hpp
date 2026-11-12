/**
 * @file mcp9700_kconfig.hpp
 * @brief Optional ESP-IDF layer: a `Config` built from the `CONFIG_MCP9700_*`
 * Kconfig options.
 * @copyright Copyright (c) 2026 The driver-mcp9700 authors.
 * SPDX-License-Identifier: MIT
 *
 * Include it only where menuconfig should choose the defaults; without Kconfig
 * `KconfigDefaults()` equals `Config{}`. One configuration per program: every
 * translation unit of one program must see the same `CONFIG_` values, and no
 * driver header includes this one (handbook KB-CPP20-018).
 */
#ifndef MCP9700_KCONFIG_HPP
#define MCP9700_KCONFIG_HPP

#ifdef __has_include
#if __has_include("sdkconfig.h")
#include "sdkconfig.h"
#endif
#endif

#include "mcp9700_types.hpp"

#ifndef CONFIG_MCP9700_ADC_CHANNEL
/** ADC channel when Kconfig does not set it: `Config{}.channel`. */
#define CONFIG_MCP9700_ADC_CHANNEL 0
#endif
#ifndef CONFIG_MCP9700_ADC_FULL_SCALE_MV
/** Declared ADC full scale when Kconfig does not set it:
 * `Config{}.adc_full_scale_mv`. */
#define CONFIG_MCP9700_ADC_FULL_SCALE_MV 3300
#endif
#ifndef CONFIG_MCP9700_SAMPLES
/** Conversions averaged when Kconfig does not set it: `Config{}.samples`. */
#define CONFIG_MCP9700_SAMPLES 8
#endif
#ifndef CONFIG_MCP9700_SAMPLE_INTERVAL_US
/** Wait between conversions when Kconfig does not set it:
 * `Config{}.sample_interval_us`. */
#define CONFIG_MCP9700_SAMPLE_INTERVAL_US 100
#endif
#ifndef CONFIG_MCP9700_OFFSET_CAL_MC
/** Calibration offset when Kconfig does not set it: `Config{}.offset_cal_mc`.
 */
#define CONFIG_MCP9700_OFFSET_CAL_MC 0
#endif
#ifndef CONFIG_MCP9700_CUSTOM_V0_MV
/** Custom output at 0 C: 0, so an unset Custom fails `Validate()`. */
#define CONFIG_MCP9700_CUSTOM_V0_MV 0
#endif
#ifndef CONFIG_MCP9700_CUSTOM_TC_UV_PER_C
/** Custom slope: 0 (unset). */
#define CONFIG_MCP9700_CUSTOM_TC_UV_PER_C 0
#endif
#ifndef CONFIG_MCP9700_CUSTOM_MIN_MC
/** Custom lowest temperature: 0 (unset). */
#define CONFIG_MCP9700_CUSTOM_MIN_MC 0
#endif
#ifndef CONFIG_MCP9700_CUSTOM_MAX_MC
/** Custom highest temperature: 0 (unset). */
#define CONFIG_MCP9700_CUSTOM_MAX_MC 0
#endif
#ifndef CONFIG_MCP9700_CUSTOM_ACCURACY_MIN_MC
/** Custom accuracy lower limit: 0 (unset). */
#define CONFIG_MCP9700_CUSTOM_ACCURACY_MIN_MC 0
#endif
#ifndef CONFIG_MCP9700_CUSTOM_ACCURACY_MAX_MC
/** Custom accuracy upper limit: 0 (unset). */
#define CONFIG_MCP9700_CUSTOM_ACCURACY_MAX_MC 0
#endif

namespace mcp9700 {

namespace detail {

/// The variant menuconfig selected (the choice sets exactly one symbol).
constexpr Variant KconfigVariant() noexcept {
#if defined(CONFIG_MCP9700_VARIANT_CUSTOM)
  return Variant::Custom;
#elif defined(CONFIG_MCP9700_VARIANT_MCP9700A)
  return Variant::Mcp9700A;
#elif defined(CONFIG_MCP9700_VARIANT_MCP9700B)
  return Variant::Mcp9700B;
#elif defined(CONFIG_MCP9700_VARIANT_MCP9701A)
  return Variant::Mcp9701A;
#elif defined(CONFIG_MCP9700_VARIANT_MCP9701)
  return Variant::Mcp9701;
#else
  return Variant::Mcp9700;
#endif
}

} // namespace detail

/**
 * @brief The configuration menuconfig selected, on top of the datasheet
 * defaults.
 * @return `Config{}` with the channel, variant, ADC full scale, averaging and
 * calibration from Kconfig; the `Custom` fields are filled only for
 * `Variant::Custom`.
 */
constexpr Config KconfigDefaults() noexcept {
  Config c{};
  c.channel = static_cast<uint8_t>(CONFIG_MCP9700_ADC_CHANNEL);
  c.variant = detail::KconfigVariant();
#if defined(CONFIG_MCP9700_HIGH_TEMPERATURE)
  c.range = TemperatureClass::High;
#endif
  if (c.variant == Variant::Custom) {
    c.custom.v0_mv = static_cast<uint32_t>(CONFIG_MCP9700_CUSTOM_V0_MV);
    c.custom.tc_uv_per_c = static_cast<uint32_t>(CONFIG_MCP9700_CUSTOM_TC_UV_PER_C);
    c.custom.min_mc = static_cast<int32_t>(CONFIG_MCP9700_CUSTOM_MIN_MC);
    c.custom.max_mc = static_cast<int32_t>(CONFIG_MCP9700_CUSTOM_MAX_MC);
    c.custom.accuracy_min_mc = static_cast<int32_t>(CONFIG_MCP9700_CUSTOM_ACCURACY_MIN_MC);
    c.custom.accuracy_max_mc = static_cast<int32_t>(CONFIG_MCP9700_CUSTOM_ACCURACY_MAX_MC);
  }
  c.adc_full_scale_mv = static_cast<uint32_t>(CONFIG_MCP9700_ADC_FULL_SCALE_MV);
  c.samples = static_cast<uint16_t>(CONFIG_MCP9700_SAMPLES);
  c.sample_interval_us = static_cast<uint32_t>(CONFIG_MCP9700_SAMPLE_INTERVAL_US);
  c.offset_cal_mc = static_cast<int32_t>(CONFIG_MCP9700_OFFSET_CAL_MC);
  return c;
}

} // namespace mcp9700

#endif // MCP9700_KCONFIG_HPP
