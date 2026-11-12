/**
 * @file host_mcp9700_port.hpp
 * @brief The host port: the driver's hooks over a simulated sensor and ADC, so the example runs on
 * any PC.
 *
 * `mcp9700_test::MockPortBase` (tests/mock_mcp9700.hpp) supplies the required ADC hooks over the
 * sensor model the host tests use. This class adds what a real port adds: `DelayUs()` (it advances
 * the model's clock instead of sleeping), `GpioSet()` for the sensor's supply pin, whose idle level
 * before the driver enables it is INACTIVE (unpowered), and `Log()` to the console. A port for a
 * real ADC on a PC (a USB DAQ, an IIO device) implements the same hooks: see docs/porting.md.
 */
#ifndef MCP9700_HOST_MCP9700_PORT_HPP
#define MCP9700_HOST_MCP9700_PORT_HPP

#include <cstdint>
#include <cstdio>

#include "mock_mcp9700.hpp"

/// The host port: simulated sensor, real console.
class HostMcp9700Port : public mcp9700_test::MockPortBase<HostMcp9700Port> {
public:
  /// A new port: the supply pin idles INACTIVE, so the sensor is off until the driver starts.
  HostMcp9700Port() noexcept {
    state.powered = false;
  }

  /// The model needs no real time; its clock moves instead.
  void DelayUs(uint32_t us) noexcept {
    state.Advance(us);
  }

  /// The sensor's supply pin: ACTIVE powers it.
  bool GpioSet(mcp9700::CtrlPin pin, mcp9700::GpioSignal signal) noexcept {
    if (pin != mcp9700::CtrlPin::Enable) {
      return false;
    }
    state.SetPower(signal == mcp9700::GpioSignal::ACTIVE);
    return true;
  }

  /// Driver messages (they carry their own `mcp9700:` prefix) go to the console, one line each.
  void Log(mcp9700::LogLevel level, const char* message) noexcept {
    (void)level;
    (void)std::puts(message); // plain text: never pass it as a format string
  }
};

#endif // MCP9700_HOST_MCP9700_PORT_HPP
