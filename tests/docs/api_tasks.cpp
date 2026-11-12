/**
 * @file api_tasks.cpp
 * @brief The API guide's and the Configuration page's examples, compiled by the host build (never
 * run).
 */
#include "mcp9700.hpp"
#include "my_port.hpp"

using Sensor = mcp9700::MCP9700<MyPort>;

// --8<-- [start:calibrated]
mcp9700::Config CalibratedConfig() {
  mcp9700::Config c = mcp9700::LowNoisePreset();
  c.channel = 2;
  c.variant = mcp9700::Variant::Mcp9700B; // the part on the board
  c.offset_cal_mc = -1200;                // measured +1.2 C high at +25 C on this board
  return c;
}

mcp9700::Error StartCalibrated(MyPort& port) {
  static Sensor sensor(port, CalibratedConfig()); // stored, no I/O
  return sensor.EnsureInitialized().error;        // InvalidArgument if the config is out of range
}
// --8<-- [end:calibrated]

// --8<-- [start:errors]
const char* Describe(mcp9700::Error e) {
  switch (e) {
  case mcp9700::Error::DeviceIdMismatch:
    return "no plausible, stable output on this channel";
  case mcp9700::Error::OutputLow:
    return "grounded or open input, unpowered sensor, wrong channel";
  case mcp9700::Error::OutputHigh:
    return "shorted to the supply, or the ADC scale is wrong";
  case mcp9700::Error::Saturated:
    return "the ADC clipped: declare its real full scale";
  case mcp9700::Error::Unstable:
    return "floating or disturbed line";
  default:
    return mcp9700::ToString(e);
  }
}
// --8<-- [end:errors]

// --8<-- [start:measurement]
bool PrintableTemperature(MyPort& port, int32_t& whole, int32_t& thousandths) {
  Sensor sensor(port);
  if (!sensor.EnsureInitialized().ok()) {
    return false;
  }
  const auto m = sensor.ReadMeasurement(); // temperature, mean VOUT and spread in one call
  if (!m.ok()) {
    return false;
  }
  const int32_t mc = m.value.temperature_mc;
  whole = mc / 1000; // integer maths: no float needed
  thousandths = ((mc < 0) ? -mc : mc) % 1000;
  return true;
}
// --8<-- [end:measurement]

// --8<-- [start:accuracy]
bool WithinDatasheetLimits(const mcp9700::Config& c, int32_t reference_mc, int32_t reading_mc) {
  const auto spec = mcp9700::ResolveSpec(c);
  if (!spec.ok()) {
    return false;
  }
  const auto lim = mcp9700::AccuracyLimitsFor(spec.value, c.range, reference_mc);
  const int32_t error_mc = reading_mc - reference_mc;
  return lim.ok() && (error_mc >= lim.value.min_mc) && (error_mc <= lim.value.max_mc);
}
// --8<-- [end:accuracy]
