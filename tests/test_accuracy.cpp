/**
 * @file test_accuracy.cpp
 * @brief `AccuracyLimitsFor()` for every band of the datasheet table (p.3), its
 * edges and its errors.
 */
#include "mcp9700.hpp"
#include "test_harness.hpp"
#include "test_util.hpp"

using mcp9700::AccuracyLimitsFor;
using mcp9700::Error;
using mcp9700::TemperatureClass;

namespace {
const mcp9700::VariantSpec& Row(std::size_t i) {
  return mcp9700::kVariantTable[i];
}
struct Case {
  std::size_t row;
  TemperatureClass cls;
  int32_t mc;
  int32_t lo;
  int32_t hi;
};
} // namespace

TEST_CASE("Every band and its edges, as the datasheet lists them") {
  const TemperatureClass e = TemperatureClass::Extended;
  const TemperatureClass h = TemperatureClass::High;
  const Case cases[] = {
      // MCP9700B
      {2, e, 20'000, -1000, 1000},
      {2, e, 70'000, -1000, 1000},
      {2, e, 70'001, -2000, 3000},
      {2, e, 19'999, -2000, 3000},
      {2, e, 0, -2000, 3000},
      {2, e, 125'000, -2000, 3000},
      {2, e, -1, -2000, 4000},
      {2, e, -40'000, -2000, 4000},
      {2, h, 125'001, -4000, 4000},
      {2, h, 150'000, -4000, 4000},
      {2, h, -40'000, -4000, 4000},
      {2, h, 25'000, -4000, 4000},
      {2, h, 70'000, -4000, 4000},
      // MCP9700A
      {1, e, 0, -2000, 2000},
      {1, e, 70'000, -2000, 2000},
      {1, e, 70'001, -2000, 4000},
      {1, e, -40'000, -2000, 4000},
      {1, e, 125'000, -2000, 4000},
      // MCP9700
      {0, e, 70'000, -4000, 4000},
      {0, e, 70'001, -4000, 6000},
      {0, e, -40'000, -4000, 6000},
      {0, h, 150'000, -4000, 6000},
      {0, h, 125'001, -4000, 6000},
      {0, h, 25'000, -4000, 6000},
      {0, h, 100'000, -4000, 6000},
      // MCP9701 and MCP9701A
      {3, e, 0, -4000, 4000},
      {3, e, -10'000, -4000, 6000},
      {3, e, 125'000, -4000, 6000},
      {4, e, 70'000, -2000, 2000},
      {4, e, -10'000, -2000, 4000},
      {4, e, 125'000, -2000, 4000},
  };
  for (const Case& c : cases) {
    const auto r = AccuracyLimitsFor(Row(c.row), c.cls, c.mc);
    REQUIRE(r.ok());
    CHECK_EQ(r.value.min_mc, c.lo);
    CHECK_EQ(r.value.max_mc, c.hi);
  }
}

TEST_CASE("Outside the specified range, and High without a High class, is "
          "InvalidArgument") {
  const TemperatureClass e = TemperatureClass::Extended;
  CHECK_EQ(AccuracyLimitsFor(Row(0), e, -40'001).error, Error::InvalidArgument);
  CHECK_EQ(AccuracyLimitsFor(Row(0), e, 125'001).error, Error::InvalidArgument);
  CHECK_EQ(AccuracyLimitsFor(Row(0), TemperatureClass::High, 150'001).error,
           Error::InvalidArgument);
  CHECK_EQ(AccuracyLimitsFor(Row(3), e, -10'001).error, Error::InvalidArgument);
  CHECK_EQ(AccuracyLimitsFor(Row(1), TemperatureClass::High, 25'000).error, Error::InvalidArgument);
  CHECK(AccuracyLimitsFor(Row(0), e, 0).value.min_mc == -4000);
}

TEST_CASE("A band gap inside the range reports InvalidArgument; Custom has its "
          "one band") {
  mcp9700::VariantSpec s{};
  s.tc_uv_per_c = 10'000;
  s.min_mc = -10'000;
  s.max_ext_mc = 50'000;
  s.max_high_mc = 50'000;
  s.band_count = 1;
  s.bands[0] = {0, 20'000, -1, 1, false};
  CHECK_EQ(AccuracyLimitsFor(s, TemperatureClass::Extended, 30'000).error, Error::InvalidArgument);
  const auto c = mcp9700::ResolveSpec(mcp9700_test::ValidCustom());
  REQUIRE(c.ok());
  const auto r = AccuracyLimitsFor(c.value, TemperatureClass::Extended, 100'000);
  REQUIRE(r.ok());
  CHECK_EQ(r.value.min_mc, -2000);
  CHECK_EQ(r.value.max_mc, 2000);
  CHECK_EQ(AccuracyLimitsFor(c.value, TemperatureClass::Extended, 125'001).error,
           Error::InvalidArgument);
  CHECK_EQ(AccuracyLimitsFor(c.value, TemperatureClass::Extended, -40'001).error,
           Error::InvalidArgument);
}
