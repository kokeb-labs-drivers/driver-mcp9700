/**
 * @file test_conversion.cpp
 * @brief The pure conversion functions against the datasheet reference points
 * and an oracle.
 */
#include "mcp9700.hpp"
#include "reference_mcp9700.hpp"
#include "test_harness.hpp"
#include "test_util.hpp"

using mcp9700::kVariantTable;
using mcp9700::MilliCToOutputMv;
using mcp9700::OutputMvToMilliC;

namespace {
const mcp9700::VariantSpec& Row(std::size_t i) {
  return kVariantTable[i];
}

// Spot values, exact with the typical coefficients (derived from Equation 4-1,
// p.10).
static_assert(OutputMvToMilliC(kVariantTable[0], 100) == -40'000, "MCP9700 100 mV");
static_assert(OutputMvToMilliC(kVariantTable[0], 500) == 0, "MCP9700 500 mV");
static_assert(OutputMvToMilliC(kVariantTable[0], 750) == 25'000, "MCP9700 750 mV");
static_assert(OutputMvToMilliC(kVariantTable[0], 1750) == 125'000, "MCP9700 1750 mV");
static_assert(OutputMvToMilliC(kVariantTable[3], 400) == 0, "MCP9701 400 mV");
static_assert(OutputMvToMilliC(kVariantTable[3], 205) == -10'000, "MCP9701 205 mV");
static_assert(MilliCToOutputMv(kVariantTable[0], 25'000) == 750U, "MCP9700 at 25 C");
static_assert(MilliCToOutputMv(kVariantTable[3], 25'000) == 888U,
              "MCP9701 at 25 C: 887.5 rounds up");
} // namespace

TEST_CASE("Reference points of both families") {
  struct P {
    int32_t mc;
    uint32_t mv9700;
    uint32_t mv9701;
  };
  const P points[] = {{-18'000, 320, 49},    {-10'000, 400, 205},   {0, 500, 400},
                      {25'000, 750, 888},    {85'000, 1350, 2058},  {125'000, 1750, 2838},
                      {133'000, 1830, 2994}, {150'000, 2000, 3325}, {158'000, 2080, 3481}};
  for (const P& p : points) {
    CHECK_EQ(MilliCToOutputMv(Row(0), p.mc), p.mv9700);
    CHECK_EQ(MilliCToOutputMv(Row(3), p.mc), p.mv9701);
    CHECK_EQ(OutputMvToMilliC(Row(0), p.mv9700), p.mc);
  }
  CHECK_EQ(MilliCToOutputMv(Row(0), -48'000), 20U);
  CHECK_EQ(MilliCToOutputMv(Row(0), -60'000), 0U); // negative output: 0
  CHECK_EQ(MilliCToOutputMv(Row(3), -40'000), 0U);
  CHECK_EQ(MilliCToOutputMv(Row(0), -50'000), 0U); // exactly 0 V
}

TEST_CASE("Rounding is half away from zero; the 9701 has 51.28 mC per mV") {
  // MCP9701: 1 mV above V0 is 1000000/19500 = 51.282 mC.
  CHECK_EQ(OutputMvToMilliC(Row(3), 401), 51);
  CHECK_EQ(OutputMvToMilliC(Row(3), 399), -51);
  CHECK_EQ(OutputMvToMilliC(Row(3), 410), 513);
  // A tie: tc 2000 uV/C, one mV is 500 mC exactly; half a mC never occurs,
  // build one with tc 4000.
  mcp9700::VariantSpec s{};
  s.v0_mv = 0;
  s.tc_uv_per_c = 4000;
  CHECK_EQ(OutputMvToMilliC(s, 1), 250);
  s.tc_uv_per_c = 1'000'000 / 3 * 2; // 666666 uV/C: 1 mV is 1.5000015 mC
  CHECK_EQ(OutputMvToMilliC(s, 1), 2);
  s.v0_mv = 1;
  CHECK_EQ(OutputMvToMilliC(s, 0), -2);
  s.tc_uv_per_c = 2'000'000; // 1 mV is exactly 0.5 mC
  s.v0_mv = 0;
  CHECK_EQ(OutputMvToMilliC(s, 1), 1);
  s.v0_mv = 1;
  CHECK_EQ(OutputMvToMilliC(s, 0), -1);
}

TEST_CASE("OutputMvToMilliC matches the oracle for every mV 0 to 4000, every "
          "variant") {
  for (std::size_t i = 0; i < 5; ++i) {
    const auto& r = mcp9700_ref::kRows[i];
    for (uint32_t mv = 0; mv <= 4000; ++mv) {
      const auto expect =
          mcp9700_ref::RoundDivScaled(static_cast<mcp9700_ref::Wide>(mv) - r.v0_mv, r.tc_uv);
      if (OutputMvToMilliC(Row(i), mv) != static_cast<int32_t>(expect)) {
        CHECK(false);
        return;
      }
    }
  }
}

TEST_CASE("MilliCToOutputMv round-trips within one millivolt") {
  for (std::size_t i = 0; i < 5; ++i) {
    for (int32_t mc = -60'000; mc <= 160'000; mc += 997) {
      const uint32_t mv = MilliCToOutputMv(Row(i), mc);
      if (mv == 0U) {
        continue;
      }
      const int32_t back = OutputMvToMilliC(Row(i), mv);
      const int64_t err_uv =
          (static_cast<int64_t>(back) - mc) * static_cast<int64_t>(Row(i).tc_uv_per_c);
      const int64_t bound = 500'000 + (static_cast<int64_t>(Row(i).tc_uv_per_c) / 2) + 1;
      CHECK((err_uv <= bound) && (err_uv >= -bound));
    }
  }
}

TEST_CASE("Extremes do not overflow: huge inputs are clamped") {
  CHECK_EQ(OutputMvToMilliC(Row(0), 0xFFFF'FFFFU), OutputMvToMilliC(Row(0), 65'535));
  mcp9700::VariantSpec s{};
  s.v0_mv = 0xFFFF'FFFFU;
  s.tc_uv_per_c = 1000;
  CHECK(MilliCToOutputMv(s, 2'000'000'000) == 0xFFFF'FFFFU);
}

TEST_CASE("ToCelsius and the constants") {
  CHECK(mcp9700::ToCelsius(25'500) == 25.5F);
  CHECK(mcp9700::ToCelsius(-40'000) == -40.0F);
  CHECK_EQ(mcp9700::kMaxOutputCurrentUa, 100U);
  CHECK_EQ(mcp9700::kMaxLoadCapacitancePf, 1000U);
  CHECK_EQ(mcp9700::kOutputImpedanceOhm, 20U);
  CHECK_EQ(mcp9700::kPowerOnTypicalUs, 800U);
}
