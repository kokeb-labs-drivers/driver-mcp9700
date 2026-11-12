/**
 * @file literal_out_of_range.cpp
 * @brief Must NOT compile: a `_u8` literal that does not fit `uint8_t` stops constant evaluation.
 *
 * EXPECT: LiteralOutOfRange
 */
#include "mcp9700_detail.hpp"

int main() {
  using mcp9700::detail::operator""_u8;
  constexpr auto kTooBig = 0x100_u8;
  return static_cast<int>(kTooBig);
}
