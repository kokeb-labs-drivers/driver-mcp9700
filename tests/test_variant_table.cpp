/**
 * @file test_variant_table.cpp
 * @brief The variant table: its own invariants, equality with the test's copy
 * of the datasheet, and the `Custom` declaration rules at every boundary.
 */
#include "mcp9700.hpp"
#include "test_harness.hpp"
#include "test_util.hpp"

using mcp9700::Config;
using mcp9700::Error;
using mcp9700::Variant;
using mcp9700_test::ValidCustom;

namespace {

constexpr bool TableInvariants() {
  for (const auto& row : mcp9700::kVariantTable) {
    if ((row.tc_uv_per_c == 0U) || (mcp9700::MilliCToOutputMv(row, row.min_mc) == 0U) ||
        (row.packages == 0U) || ((row.high_packages & row.packages) != row.high_packages) ||
        (row.band_count == 0U) || (row.band_count > mcp9700::kMaxBands)) {
      return false;
    }
    if (row.has_high != (row.max_high_mc > row.max_ext_mc)) {
      return false;
    }
    bool has_high_band = false;
    for (std::size_t i = 0; i < row.band_count; ++i) {
      const auto& b = row.bands[i];
      const int32_t top = b.high_only ? row.max_high_mc : row.max_ext_mc;
      if ((b.from_mc < row.min_mc) || (b.to_mc > top) || (b.min_mc > 0) || (b.max_mc < 0)) {
        return false;
      }
      has_high_band = has_high_band || b.high_only;
      if ((i > 0U) &&
          ((b.to_mc - b.from_mc) < (row.bands[i - 1U].to_mc - row.bands[i - 1U].from_mc))) {
        return false; // narrowest first
      }
    }
    if (has_high_band != row.has_high) {
      return false;
    }
  }
  return true;
}
static_assert(TableInvariants(), "every row of the variant table keeps its invariants");

struct Expect {
  uint32_t v0, tc;
  int32_t min, ext, high;
  bool has_high;
  uint32_t vmin, vmax, tested;
  uint8_t packages, high_packages;
  uint8_t bands;
};

constexpr uint8_t kAll = 7;
constexpr uint8_t kSmall = 3;
// The datasheet rows again, written out here (DS20001942L p.3, p.4, p.12,
// p.23).
constexpr Expect kExpect[5] = {
    {500, 10000, -40000, 125000, 150000, true, 2300, 5500, 3300, kAll, kSmall, 3},
    {500, 10000, -40000, 125000, 125000, false, 2300, 5500, 3300, kAll, 0, 2},
    {500, 10000, -40000, 125000, 150000, true, 2300, 5500, 3300, kSmall, kSmall, 4},
    {400, 19500, -10000, 125000, 125000, false, 3100, 5500, 5000, kAll, 0, 2},
    {400, 19500, -10000, 125000, 125000, false, 3100, 5500, 5000, kAll, 0, 2},
};

struct Band {
  int32_t from, to, lo, hi;
  bool high;
};
constexpr Band kBands[5][4] = {
    {{0, 70000, -4000, 4000, false},
     {-40000, 125000, -4000, 6000, false},
     {-40000, 150000, -4000, 6000, true},
     {}},
    {{0, 70000, -2000, 2000, false}, {-40000, 125000, -2000, 4000, false}, {}, {}},
    {{20000, 70000, -1000, 1000, false},
     {0, 125000, -2000, 3000, false},
     {-40000, 125000, -2000, 4000, false},
     {-40000, 150000, -4000, 4000, true}},
    {{0, 70000, -4000, 4000, false}, {-10000, 125000, -4000, 6000, false}, {}, {}},
    {{0, 70000, -2000, 2000, false}, {-10000, 125000, -2000, 4000, false}, {}, {}},
};

Error V(const Config& c) {
  return mcp9700::Validate(c);
}

} // namespace

TEST_CASE("The table equals the datasheet rows value by value") {
  for (std::size_t i = 0; i < 5; ++i) {
    const auto& r = mcp9700::kVariantTable[i];
    const auto& e = kExpect[i];
    CHECK_EQ(r.v0_mv, e.v0);
    CHECK_EQ(r.tc_uv_per_c, e.tc);
    CHECK_EQ(r.min_mc, e.min);
    CHECK_EQ(r.max_ext_mc, e.ext);
    CHECK_EQ(r.max_high_mc, e.high);
    CHECK_EQ(r.has_high, e.has_high);
    CHECK_EQ(r.vdd_min_mv, e.vmin);
    CHECK_EQ(r.vdd_max_mv, e.vmax);
    CHECK_EQ(r.tested_vdd_mv, e.tested);
    CHECK_EQ(r.packages, e.packages);
    CHECK_EQ(r.high_packages, e.high_packages);
    CHECK_EQ(r.band_count, e.bands);
    for (std::size_t b = 0; b < e.bands; ++b) {
      CHECK_EQ(r.bands[b].from_mc, kBands[i][b].from);
      CHECK_EQ(r.bands[b].to_mc, kBands[i][b].to);
      CHECK_EQ(r.bands[b].min_mc, kBands[i][b].lo);
      CHECK_EQ(r.bands[b].max_mc, kBands[i][b].hi);
      CHECK_EQ(r.bands[b].high_only, kBands[i][b].high);
    }
    // The independent model rows agree with the same numbers.
    const auto& m = mcp9700_test::chip::kRows[i];
    CHECK(m.v0_mv == static_cast<double>(e.v0));
    CHECK(m.tc_mv_per_c * 1000.0 == static_cast<double>(e.tc));
    CHECK(m.has_high == e.has_high);
  }
}

TEST_CASE("ConfigFor passes Validate for every named variant, and GetSpec "
          "returns the row") {
  for (std::size_t i = 0; i < 5; ++i) {
    const Config c = mcp9700_test::NamedConfig(i);
    CHECK_EQ(V(c), Error::Ok);
    const auto spec = mcp9700::ResolveSpec(c);
    REQUIRE(spec.ok());
    CHECK_EQ(spec.value.tc_uv_per_c, mcp9700::kVariantTable[i].tc_uv_per_c);
  }
  CHECK_EQ(V(mcp9700::Mcp9701Preset()), Error::Ok);
  CHECK_EQ(V(mcp9700::HighTemperaturePreset()), Error::Ok);
}

TEST_CASE("Custom: the transfer function limits at every boundary") {
  Config c = ValidCustom();
  CHECK_EQ(V(c), Error::Ok);
  for (const uint32_t tc : {999U, 100'001U}) {
    c = ValidCustom();
    c.custom.tc_uv_per_c = tc;
    CHECK_EQ(V(c), Error::InvalidArgument);
  }
  c = ValidCustom();
  c.custom.tc_uv_per_c = 1000;
  c.custom.min_mc = 0;
  c.custom.max_mc = 100'000;
  CHECK_EQ(V(c), Error::Ok);
  c.custom.tc_uv_per_c = 100'000;
  c.custom.v0_mv = 0;
  c.custom.min_mc = 0;
  c.custom.max_mc = 200'000; // 20000 mV exactly
  CHECK_EQ(V(c), Error::Ok);
  c.custom.v0_mv = 1; // 20001 mV
  CHECK_EQ(V(c), Error::InvalidArgument);
  c = ValidCustom();
  c.custom.v0_mv = 5000;
  c.custom.tc_uv_per_c = 1000;
  c.custom.min_mc = 0;
  c.custom.max_mc = 100'000;
  CHECK_EQ(V(c), Error::Ok);
  c.custom.v0_mv = 5001;
  CHECK_EQ(V(c), Error::InvalidArgument);
  c = ValidCustom();
  c.custom.max_mc = c.custom.min_mc;
  CHECK_EQ(V(c), Error::InvalidArgument);
  c.custom.max_mc = c.custom.min_mc - 1;
  CHECK_EQ(V(c), Error::InvalidArgument);
  c = ValidCustom();
  c.custom.v0_mv = 0;
  c.custom.min_mc = -1000; // negative output at the lowest temperature
  CHECK_EQ(V(c), Error::InvalidArgument);
  c = ValidCustom();
  c.custom.min_mc = -200'001;
  c.custom.v0_mv = 5000;
  c.custom.tc_uv_per_c = 1000;
  CHECK_EQ(V(c), Error::InvalidArgument);
  c = ValidCustom();
  c.custom.v0_mv = 5000;
  c.custom.tc_uv_per_c = 1000;
  c.custom.min_mc = -5000;
  c.custom.max_mc = 200'001;
  CHECK_EQ(V(c), Error::InvalidArgument);
  c.custom.max_mc = 200'000;
  CHECK_EQ(V(c), Error::Ok);
  c.custom.max_mc = -200'001;
  c.custom.min_mc = -200'002;
  CHECK_EQ(V(c), Error::InvalidArgument);
}

TEST_CASE("Custom: the accuracy declaration") {
  Config c = ValidCustom();
  c.custom.accuracy_min_mc = 0;
  c.custom.accuracy_max_mc = 0;
  CHECK_EQ(V(c), Error::InvalidArgument);
  c.custom.accuracy_max_mc = 1;
  CHECK_EQ(V(c), Error::Ok);
  c.custom.accuracy_min_mc = 1;
  CHECK_EQ(V(c), Error::InvalidArgument);
  c = ValidCustom();
  c.custom.accuracy_max_mc = -1;
  CHECK_EQ(V(c), Error::InvalidArgument);
  c = ValidCustom();
  c.custom.accuracy_min_mc = -20'000;
  c.custom.accuracy_max_mc = 20'000;
  CHECK_EQ(V(c), Error::Ok);
  c.custom.accuracy_min_mc = -20'001;
  CHECK_EQ(V(c), Error::InvalidArgument);
  c.custom.accuracy_min_mc = -20'000;
  c.custom.accuracy_max_mc = 20'001;
  CHECK_EQ(V(c), Error::InvalidArgument);
}

TEST_CASE("Custom: an unset declaration, High and a bad variant are rejected") {
  Config c{};
  c.variant = Variant::Custom;
  CHECK_EQ(V(c), Error::InvalidArgument);
  c = ValidCustom();
  c.range = mcp9700::TemperatureClass::High;
  CHECK_EQ(V(c), Error::InvalidArgument);
  c = ValidCustom();
  CHECK_EQ(mcp9700::ResolveSpec(c).ok(), true);
  c.custom.tc_uv_per_c = 0;
  CHECK_EQ(mcp9700::ResolveSpec(c).error, Error::InvalidArgument);
}

TEST_CASE("A named variant rejects every non-zero custom field, one test per field") {
  for (std::size_t field = 0; field < 6; ++field) {
    Config c{};
    CHECK_EQ(V(c), Error::Ok);
    switch (field) {
    case 0:
      c.custom.v0_mv = 1;
      break;
    case 1:
      c.custom.tc_uv_per_c = 1;
      break;
    case 2:
      c.custom.min_mc = 1;
      break;
    case 3:
      c.custom.max_mc = 1;
      break;
    case 4:
      c.custom.accuracy_min_mc = -1;
      break;
    default:
      c.custom.accuracy_max_mc = 1;
      break;
    }
    CHECK_EQ(V(c), Error::InvalidArgument);
  }
}

TEST_CASE("High is accepted only where the row has it") {
  const bool expect[5] = {true, false, true, false, false};
  for (std::size_t i = 0; i < 5; ++i) {
    Config c = mcp9700_test::NamedConfig(i);
    c.range = mcp9700::TemperatureClass::High;
    CHECK_EQ(V(c) == Error::Ok, expect[i]);
  }
}
