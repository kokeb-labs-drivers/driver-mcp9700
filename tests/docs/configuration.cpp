/**
 * @file configuration.cpp
 * @brief The Configuration page's examples, compiled by the host build (never run).
 */
#include "mcp9700.hpp"
#include "mcp9700_kconfig.hpp"
#include "my_port.hpp"

// --8<-- [start:kconfig]
mcp9700::Error StartFromMenuconfig(MyPort& port) {
  static mcp9700::MCP9700<MyPort> sensor(port, mcp9700::KconfigDefaults());
  return sensor.EnsureInitialized().error;
}
// --8<-- [end:kconfig]

// --8<-- [start:named]
constexpr mcp9700::Config Mcp9701OnA5VAdc() {
  mcp9700::Config c = mcp9700::ConfigFor(mcp9700::Variant::Mcp9701);
  c.adc_full_scale_mv = 5000; // the largest voltage the port reports
  c.adc_resolution_bits = 10;
  return c;
}
// --8<-- [end:named]

// --8<-- [start:divider]
constexpr mcp9700::Config BehindADivider() {
  mcp9700::Config c{};
  c.divider_num = 3; // pin = VOUT x 2/3 (two resistors): VOUT = pin x 3/2
  c.divider_den = 2;
  return c;
}
// --8<-- [end:divider]

// --8<-- [start:custom-offset]
// Values from your own datasheet; the driver does not know this part: 750 mV at 0 C, 10 mV/C,
// specified from -40 to +125 C, error between -2 and +3 C. Output at the two ends:
// 750 - 400 = 350 mV and 750 + 1250 = 2000 mV, both inside the 0 to 20000 mV window.
constexpr mcp9700::Config CustomPart750() {
  mcp9700::Config c{};
  c.variant = mcp9700::Variant::Custom;
  c.custom.v0_mv = 750;
  c.custom.tc_uv_per_c = 10'000;
  c.custom.min_mc = -40'000;
  c.custom.max_mc = 125'000;
  c.custom.accuracy_min_mc = -2000;
  c.custom.accuracy_max_mc = 3000;
  return c;
}
// --8<-- [end:custom-offset]

// --8<-- [start:custom-zero]
// A part that outputs 0 mV at 0 C and 10 mV/C, specified 0 to +150 C: it outputs 0 mV at its own
// minimum, so a grounded line reads as a valid 0 C (docs/limitations.md).
constexpr mcp9700::Config CustomPartZero() {
  mcp9700::Config c{};
  c.variant = mcp9700::Variant::Custom;
  c.custom.v0_mv = 0;
  c.custom.tc_uv_per_c = 10'000;
  c.custom.min_mc = 0;
  c.custom.max_mc = 150'000;
  c.custom.accuracy_min_mc = -2000;
  c.custom.accuracy_max_mc = 2000;
  return c;
}
// --8<-- [end:custom-zero]

static_assert(mcp9700::Validate(Mcp9701OnA5VAdc()) == mcp9700::Error::Ok,
              "the page's examples are valid");
static_assert(mcp9700::Validate(BehindADivider()) == mcp9700::Error::Ok,
              "the page's examples are valid");
static_assert(mcp9700::Validate(CustomPart750()) == mcp9700::Error::Ok,
              "the page's examples are valid");
static_assert(mcp9700::Validate(CustomPartZero()) == mcp9700::Error::Ok,
              "the page's examples are valid");
