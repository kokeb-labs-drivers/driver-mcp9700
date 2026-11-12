/**
 * @file named_variant_with_custom_checked.cpp
 * @brief Must NOT compile: a named variant with free coefficients (here a 9701 with a slope of its
 * own) is rejected by the same static_assert.
 *
 * EXPECT: ConfigIsInvalid
 */
#include "mcp9700_types.hpp"

constexpr mcp9700::Config MixedBoard() {
  mcp9700::Config c = mcp9700::Mcp9701Preset();
  c.custom.tc_uv_per_c = 10'000;
  return c;
}

static_assert(mcp9700::Validate(MixedBoard()) == mcp9700::Error::Ok, "ConfigIsInvalid");

int main() {
  return 0;
}
