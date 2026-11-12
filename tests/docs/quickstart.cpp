/**
 * @file quickstart.cpp
 * @brief The Quickstart page's example, compiled by the host build (never run).
 */
#include "my_port.hpp"

// --8<-- [start:quickstart]
#include "mcp9700.hpp"

int read_board_temperature(MyPort& port, int32_t& milli_celsius) {
  mcp9700::MCP9700<MyPort> sensor(port); // no I/O yet: an MCP9700 on a 12 bit, 3.3 V ADC

  if (auto r = sensor.EnsureInitialized(); !r) { // 1. ADC up, settle, one validated reading
    return static_cast<int>(r.error);            //    DeviceIdMismatch: wrong channel or no sensor
  }
  const auto t = sensor.ReadTemperatureMilliC(); // 2. discard 1, average 8, check, convert
  if (!t) {
    return static_cast<int>(t.error); //    OutputLow, OutputHigh, Saturated, Unstable or Bus
  }
  milli_celsius = t.value; // 3. 25000 is 25.000 C
  return 0;
}
// --8<-- [end:quickstart]
