/**
 * @file config_variant_invalid_checked.cpp
 * @brief Must NOT compile: a constant configuration with a variant that is not an enumerator fails
 * the static_assert that an application can put next to it.
 *
 * EXPECT: ConfigIsInvalid
 */
#include "mcp9700_types.hpp"

constexpr mcp9700::Config BadBoard() {
  mcp9700::Config c{};
  c.variant = static_cast<mcp9700::Variant>(9);
  return c;
}

static_assert(mcp9700::Validate(BadBoard()) == mcp9700::Error::Ok, "ConfigIsInvalid");

int main() {
  return 0;
}
