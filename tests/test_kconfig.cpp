/**
 * @file test_kconfig.cpp
 * @brief KconfigDefaults(): Config{} without Kconfig, the menuconfig values
 * with it (KCONFIG_SET).
 */
#include "mcp9700_kconfig.hpp"
#include "test_harness.hpp"

#if defined(KCONFIG_SET)
TEST_CASE("Kconfig values reach the Config, and the result validates") {
  constexpr mcp9700::Config kC = mcp9700::KconfigDefaults();
  CHECK_EQ(kC.channel, 2);
  CHECK_EQ(kC.samples, 16);
  CHECK(kC.variant == mcp9700::Variant::Custom);
  CHECK_EQ(kC.custom.v0_mv, 750U);
  CHECK_EQ(kC.custom.tc_uv_per_c, 10'000U);
  CHECK_EQ(kC.custom.min_mc, -40'000);
  CHECK_EQ(kC.custom.max_mc, 125'000);
  CHECK_EQ(kC.custom.accuracy_min_mc, -2000);
  CHECK_EQ(kC.custom.accuracy_max_mc, 3000);
  CHECK_EQ(kC.adc_full_scale_mv, 3100U);
  CHECK_EQ(kC.sample_interval_us, 50U);
  CHECK_EQ(kC.offset_cal_mc, -500);
  CHECK(mcp9700::Validate(kC) == mcp9700::Error::Ok);
}
#elif defined(KCONFIG_HIGH)
TEST_CASE("The MCP9700B with the High option from Kconfig") {
  constexpr mcp9700::Config kC = mcp9700::KconfigDefaults();
  CHECK(kC.variant == mcp9700::Variant::Mcp9700B);
  CHECK(kC.range == mcp9700::TemperatureClass::High);
  CHECK(mcp9700::Validate(kC) == mcp9700::Error::Ok);
}
#elif defined(KCONFIG_9701A)
TEST_CASE("Each variant symbol selects its variant") {
  CHECK(mcp9700::KconfigDefaults().variant == mcp9700::Variant::Mcp9701A);
  CHECK(mcp9700::Validate(mcp9700::KconfigDefaults()) == mcp9700::Error::Ok);
}
#elif defined(KCONFIG_9701) || defined(KCONFIG_9700A)
TEST_CASE("Each variant symbol selects its variant (second set)") {
#if defined(KCONFIG_9701)
  CHECK(mcp9700::KconfigDefaults().variant == mcp9700::Variant::Mcp9701);
#else
  CHECK(mcp9700::KconfigDefaults().variant == mcp9700::Variant::Mcp9700A);
#endif
}
#else
TEST_CASE("Without Kconfig, KconfigDefaults() equals Config{}") {
  constexpr mcp9700::Config kC = mcp9700::KconfigDefaults();
  constexpr mcp9700::Config kD{};
  CHECK_EQ(kC.channel, kD.channel);
  CHECK(kC.variant == kD.variant);
  CHECK(kC.range == kD.range);
  CHECK_EQ(kC.adc_full_scale_mv, kD.adc_full_scale_mv);
  CHECK_EQ(kC.samples, kD.samples);
  CHECK_EQ(kC.sample_interval_us, kD.sample_interval_us);
  CHECK_EQ(kC.offset_cal_mc, kD.offset_cal_mc);
  CHECK_EQ(kC.custom.tc_uv_per_c, kD.custom.tc_uv_per_c);
}
#endif
