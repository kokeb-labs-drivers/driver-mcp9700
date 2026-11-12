/**
 * @file test_util.hpp
 * @brief Small helpers shared by the host tests: configurations of every
 * variant and a custom one.
 */
#ifndef MCP9700_TEST_UTIL_HPP
#define MCP9700_TEST_UTIL_HPP

#include <bit>
#include <cstddef>
#include <cstdint>

#include "mcp9700.hpp"
#include "mock_mcp9700.hpp"

namespace mcp9700_test {

/// An enumeration value with the raw byte `raw`, enumerator or not (bit_cast:
/// an out-of-range static_cast is not valid).
template <typename E>
inline E RawEnum(uint8_t raw) noexcept {
  static_assert(sizeof(E) == sizeof(uint8_t), "the enumeration is one byte");
  return std::bit_cast<E>(raw);
}

/// The named variant with index `i` (0 to 4).
inline mcp9700::Config NamedConfig(std::size_t i) {
  return mcp9700::ConfigFor(static_cast<mcp9700::Variant>(i));
}

/// A `Custom` configuration for the synthetic part `s`, with the accuracy +-2
/// C.
inline mcp9700::Config CustomConfig(const chip::Synthetic& s) {
  mcp9700::Config c{};
  c.variant = mcp9700::Variant::Custom;
  c.custom.v0_mv = s.v0_mv;
  c.custom.tc_uv_per_c = s.tc_uv_per_c;
  c.custom.min_mc = s.min_mc;
  c.custom.max_mc = s.max_mc;
  c.custom.accuracy_min_mc = -2000;
  c.custom.accuracy_max_mc = 2000;
  return c;
}

/// A port whose conversions are scripted: the values repeat the last one when
/// the script ends.
class ScriptPort : public mcp9700::AdcInterface<ScriptPort> {
public:
  uint32_t values[128]{750}; ///< The script.
  std::size_t count = 1;     ///< Entries in use (at least 1).
  std::size_t next = 0;      ///< The next entry.
  int conversions = 0;       ///< Conversions so far.
  int fail_at = -1;          ///< Fail this conversion (0-based).

  /// Every conversion returns `v` from now on.
  void Set(uint32_t v) noexcept {
    values[0] = v;
    count = 1;
    next = 0;
  }
  bool EnsureInitialized() noexcept {
    return true;
  }
  bool ReadVoltageMv(uint8_t channel, uint32_t& mv) noexcept {
    (void)channel;
    const int index = conversions++;
    if (index == fail_at) {
      return false;
    }
    mv = values[(next < count) ? next : (count - 1U)];
    next++;
    return true;
  }
  void DelayUs(uint32_t us) noexcept {
    (void)us;
  }
};

/// A valid `Custom` declaration to bend one field of.
inline mcp9700::Config ValidCustom() {
  return CustomConfig(chip::kSynthetics[1]);
}

} // namespace mcp9700_test

#endif // MCP9700_TEST_UTIL_HPP
