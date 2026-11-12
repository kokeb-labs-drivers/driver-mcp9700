/**
 * @file mcp9700.hpp
 * @brief MCP970x driver: the one header to include.
 * @copyright Copyright (c) 2026 The driver-mcp9700 authors.
 * SPDX-License-Identifier: MIT
 *
 * @details
 * `mcp9700::MCP9700<Port>` reads one MCP9700, MCP9700A, MCP9700B, MCP9701 or
 * MCP9701A analog temperature sensor (or a declared first-order `Custom` part)
 * through an ADC channel of your port (see `mcp9700_adc_interface.hpp`).
 * Integers only, no platform code, no heap, no exceptions, no lock.
 */
#ifndef MCP9700_HPP
#define MCP9700_HPP

#include <cstdint>
#include <limits>

#include "mcp9700_adc_interface.hpp"
#include "mcp9700_detail.hpp"
#include "mcp9700_types.hpp"
#include "mcp9700_version.h"

namespace mcp9700 {

namespace detail {

// Optional hooks: a `Declares*` concept only tests the name, the `Has*` concept
// the exact signature, `noexcept` and the result type; the driver rejects a
// hook that is declared but does not match (handbook KB-CPP20-007).

/// The port declares something called `GpioSet` (used to reject a misdeclared
/// hook).
template <typename Port>
concept DeclaresGpioSet =
    requires(Port& port) { port.GpioSet(CtrlPin::Enable, GpioSignal::ACTIVE); };

/// The port drives control pins: `bool GpioSet(CtrlPin, GpioSignal) noexcept`.
template <typename Port>
concept HasGpioSet =
    requires { ExactHook<Port, bool(CtrlPin, GpioSignal) noexcept>::Take(&Port::GpioSet); };

/// The port declares something called `DelayUs`.
template <typename Port>
concept DeclaresDelayUs = requires(Port& port) { port.DelayUs(1_u32); };

/// The port waits: `void DelayUs(uint32_t us) noexcept`.
template <typename Port>
concept HasDelayUs = requires { ExactHook<Port, void(uint32_t) noexcept>::Take(&Port::DelayUs); };

/// The port declares something called `Log`.
template <typename Port>
concept DeclaresLog = requires(Port& port) { port.Log(LogLevel::Info, ""); };

/// The port takes log lines: `void Log(LogLevel, const char* message)
/// noexcept`.
template <typename Port>
concept HasLog =
    requires { ExactHook<Port, void(LogLevel, const char*) noexcept>::Take(&Port::Log); };

/// Wait through the port; a port without `DelayUs()` fails to compile at the
/// first wait.
template <typename Port>
void DelayUs(Port& port, uint32_t us) noexcept {
  if constexpr (HasDelayUs<Port>) {
    port.DelayUs(us);
  } else {
    static_assert(kAlwaysFalse<Port>, "mcp9700: the port must implement `void "
                                      "DelayUs(uint32_t us) noexcept`");
  }
}

/// Log through the port, or nothing when the port has no `Log()`.
template <typename Port>
void Log([[maybe_unused]] Port& port, [[maybe_unused]] LogLevel level,
         [[maybe_unused]] const char* message) noexcept {
  if constexpr (HasLog<Port>) {
    port.Log(level, message);
  }
}

/// The conversions of one reading: sum, smallest and largest, at the ADC pin.
struct Samples {
  uint64_t sum{0U};                                   ///< Sum of the real (not dummy) conversions.
  uint32_t min{std::numeric_limits<uint32_t>::max()}; ///< Smallest conversion.
  uint32_t max{0U};                                   ///< Largest conversion.
};

/// `ceil(a / b)` for a positive `b`.
constexpr uint64_t CeilDiv(uint64_t a, uint64_t b) noexcept {
  return (a + b - 1U) / b;
}

/// The number of ADC codes, `2^bits` (bits are 8 to 16 in a valid
/// configuration).
constexpr uint64_t AdcCodes(const Config& config) noexcept {
  return uint64_t{1} << (static_cast<uint32_t>(config.adc_resolution_bits) & 0x3FU);
}

/**
 * @brief One ADC step in millivolts at the pin, rounded up, at least 1:
 * `ceil(full_scale / 2^bits)`.
 * @param config A valid configuration.
 * @return The step.
 */
constexpr uint64_t AdcLsbMv(const Config& config) noexcept {
  const uint64_t lsb = CeilDiv(config.adc_full_scale_mv, AdcCodes(config));
  return (lsb > 0U) ? lsb : uint64_t{1};
}

/**
 * @brief The reading checks of `ReadMeasurement()`: saturation, spread,
 * scaling, window.
 * @param config A valid configuration.
 * @param spec Its variant row.
 * @param samples The conversions of the reading.
 * @return The measurement (temperature before `offset_cal_mc`), or `Saturated`,
 * `Unstable`, `OutputLow` or `OutputHigh`.
 */
inline Result<Measurement> Evaluate(const Config& config, const VariantSpec& spec,
                                    const Samples& samples) noexcept;

/// The log text of a start-up check that failed with `error`.
constexpr const char* StartupReason(Error error) noexcept {
  if (error == Error::OutputLow) {
    return "mcp9700: start-up output too low: unpowered, grounded or wrong "
           "channel";
  }
  if (error == Error::OutputHigh) {
    return "mcp9700: start-up output too high: shorted to supply or mis-scaled "
           "ADC";
  }
  if (error == Error::Saturated) {
    return "mcp9700: start-up conversion at the ADC full scale: clipped or "
           "open input";
  }
  return "mcp9700: start-up output unstable: floating or disturbed line";
}

} // namespace detail

/**
 * @defgroup mcp9700_driver MCP970x driver
 * @brief The driver class.
 * @{
 */

/**
 * @brief Driver for one MCP970x analog temperature sensor (or a declared
 * `Custom` part) on an ADC port.
 *
 * Lifecycle: construct with the configuration (no I/O), then
 * `EnsureInitialized()`, the one way to start, use, optionally `Deinitialize()`
 * or `Sleep()`. Every call that touches the sensor returns
 * `Error::NotInitialized` until `EnsureInitialized()` has succeeded; no call
 * starts it as a side effect.
 *
 * Hooks beyond the two required ADC ones: `void DelayUs(uint32_t)` (required by
 * every wait: a missing one is a compile error), `bool GpioSet(CtrlPin,
 * GpioSignal)` (the sensor's supply from an MCU pin; without it `Sleep()`
 * returns `Unsupported`), `void Log(LogLevel, const char*)`.
 *
 * A reading never retries and never clamps: a failed read is reported once, and
 * a failed read does not uninitialise the driver.
 *
 * @tparam Port Your class derived from `AdcInterface<Port>`.
 */
template <AdcPort Port>
class MCP9700 {
  static_assert(!detail::DeclaresGpioSet<Port> || detail::HasGpioSet<Port>,
                "mcp9700: the optional GpioSet hook must be `bool "
                "GpioSet(CtrlPin, GpioSignal) "
                "noexcept`, so that a failed pin write is reported");
  static_assert(!detail::DeclaresDelayUs<Port> || detail::HasDelayUs<Port>,
                "mcp9700: the DelayUs hook must be `void DelayUs(uint32_t us) "
                "noexcept`, so that no "
                "wait is truncated");
  static_assert(!detail::DeclaresLog<Port> || detail::HasLog<Port>,
                "mcp9700: the optional Log hook must be `void Log(LogLevel, "
                "const char* message) "
                "noexcept`");

public:
  /**
   * @brief Bind the driver to a port and store the configuration. No I/O,
   * cannot fail.
   * @param port The port; it must outlive the driver.
   * @param config The sensor and the ADC in front of it, checked by
   * `EnsureInitialized()` before any I/O.
   */
  explicit MCP9700(Port& port, const Config& config = {}) noexcept : port_(port), config_(config) {}

  /** @brief One driver object per sensor. */
  MCP9700(const MCP9700&) = delete;
  /** @brief One driver object per sensor. */
  MCP9700& operator=(const MCP9700&) = delete;

  /**
   * @brief Start the sensor, once: the only way to initialise the driver.
   *
   * Checks the configuration, brings the ADC up, powers the sensor (`GpioSet`,
   * when the port has it), waits `power_on_settle_us` (the datasheet gives t_ON
   * typical only, 800 us), then takes one validated reading: the analog
   * equivalent of a chip-ID check. After it succeeds, later calls return `Ok`
   * at once with no I/O. After it fails the sensor is switched off again (best
   * effort), the driver stays uninitialised and the next call starts again.
   *
   * @return `Ok`, `InvalidArgument` (the configuration fails `Validate()`; no
   * I/O), `Bus`, or `DeviceIdMismatch` (no plausible, stable output on the
   * channel).
   */
  Result<void> EnsureInitialized() noexcept;

  /** @return true after a successful `EnsureInitialized()`. No I/O. */
  [[nodiscard]] bool IsInitialized() const noexcept {
    return initialized_;
  }

  /**
   * @brief Mark the driver uninitialised and switch the sensor off when the
   * port has `GpioSet()`; the next `EnsureInitialized()` starts it again (the
   * recovery after a fault). `Ok` with no I/O when it was not initialised.
   * @return `Ok` or `Bus` (the pin write failed; the driver is uninitialised
   * either way and the sensor may still be powered). A second `Deinitialize()`
   * does nothing; to retry the switch-off call `EnsureInitialized()` and then
   * `Sleep()` or `Deinitialize()` again.
   */
  Result<void> Deinitialize() noexcept;

  /**
   * @brief Switch the sensor off; `EnsureInitialized()` powers it again.
   * @return `Ok`, `NotInitialized`, `Unsupported` (no `GpioSet()`) or `Bus`
   * (the pin write failed: see `Deinitialize()` for the retry).
   */
  Result<void> Sleep() noexcept;

  /**
   * @brief The validated reading: `discard_conversions` dummy conversions, then
   * `samples` averaged.
   *
   * Sends exactly `discard_conversions + samples` conversions, never retries.
   * Checks, in this order of precedence: `Bus` (stops at the first failed
   * conversion), `Saturated`, `Unstable`, then `OutputLow` / `OutputHigh`
   * against the specified range plus `range_margin_mc`.
   *
   * @return The measurement, or `NotInitialized`, `Bus`, `Saturated`,
   * `Unstable`, `OutputLow` or `OutputHigh`.
   */
  Result<Measurement> ReadMeasurement() noexcept;

  /**
   * @brief The temperature of `ReadMeasurement()`, milli-degrees Celsius, after
   * `offset_cal_mc`.
   * @return The temperature, or the errors of `ReadMeasurement()`.
   */
  Result<int32_t> ReadTemperatureMilliC() noexcept;

  /**
   * @brief The mean output voltage at VOUT (scaled by the divider), validated
   * like a reading.
   * @return Millivolts, or the errors of `ReadMeasurement()`.
   */
  Result<uint32_t> ReadOutputMv() noexcept;

  /**
   * @brief One conversion, once: no scaling, no check, no retry (handbook
   * KB-CPP-129). For bring-up.
   * @return Millivolts at the ADC pin as the port reported them, or
   * `NotInitialized` or `Bus`.
   */
  Result<uint32_t> ReadRawPortMv() noexcept;

  /** @return The last single conversion any read made, millivolts at the pin; 0
   * before the first, after a failed conversion and after `Deinitialize()`/`Sleep()`.
   * No I/O. */
  [[nodiscard]] uint32_t GetLastRawPortMv() const noexcept {
    return last_raw_mv_;
  }

  /**
   * @brief One ADC step expressed in milli-degrees at the current slope,
   * rounded up. No I/O.
   * @return The step, or 0 when the configuration is invalid.
   */
  [[nodiscard]] uint32_t ResolutionMilliC() const noexcept;

  /** @return The configuration stored by the constructor. No I/O. */
  [[nodiscard]] const Config& GetConfig() const& noexcept {
    return config_;
  }
  /** @brief Not on a temporary driver: the reference would dangle (handbook
   * KB-CPP20-044). */
  void GetConfig() const&& = delete;

  /**
   * @brief The effective variant row (a table row, or the one built from the
   * `Custom` declaration). No I/O.
   * @return The row, or `InvalidArgument` when the configuration is invalid.
   */
  [[nodiscard]] Result<VariantSpec> GetSpec() const noexcept {
    return ResolveSpec(config_);
  }

private:
  Result<void> initialize() noexcept;
  Result<void> failStartup(Error reason) noexcept;
  Result<void> switchOff() noexcept;
  Result<detail::Samples> collect() noexcept;
  Result<Measurement> readValidated() noexcept;
  Result<uint32_t> readOnce() noexcept;

  Port& port_;
  Config config_{};
  VariantSpec spec_{};       ///< The row `EnsureInitialized()` resolved.
  uint32_t last_raw_mv_{0U}; ///< The last single conversion (`GetLastRawPortMv()`).
  bool initialized_{false};
};

/** @} */

} // namespace mcp9700

// codeql::misra_deviation_next_line(MD-CPP-009)
#include "../src/mcp9700.ipp" // IWYU pragma: export

#endif // MCP9700_HPP
