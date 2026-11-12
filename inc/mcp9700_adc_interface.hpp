/**
 * @file mcp9700_adc_interface.hpp
 * @brief The ADC port of the MCP970x driver: the CRTP base, its pin enums, the `AdcPort` concept
 * and the output-load limits a port must respect.
 * @copyright Copyright (c) 2026 The driver-mcp9700 authors. SPDX-License-Identifier: MIT
 *
 * @details
 * The sensor has an analog output read through an ADC channel of your MCU. The driver calls hooks
 * on a class you derive from `mcp9700::AdcInterface<Self>`:
 *
 * Required: `bool EnsureInitialized() noexcept` (bring the ADC up on the first call) and
 * `bool ReadVoltageMv(uint8_t channel, uint32_t& voltage_mv) noexcept` (one calibrated conversion
 * in millivolts at the ADC pin; `false` on any failure). `void DelayUs(uint32_t us) noexcept` is
 * required in effect: the first wait is a compile error without it. Optional: `bool
 * GpioSet(CtrlPin, GpioSignal) noexcept` (powers the sensor from an MCU pin, DS20001942L
 * Section 4.2, p.11; idle INACTIVE before the output is enabled) and `void Log(LogLevel, const
 * char*) noexcept` (plain text, never a format). The full contract of each hook is the table in
 * `docs/porting.md`.
 *
 * The hook is the standard millivolt hook of the driver contract, so one ADC port serves every
 * analog-input driver. The port exposes no raw counts; the application declares the port's full
 * scale and resolution in `Config`.
 */
#ifndef MCP9700_ADC_INTERFACE_HPP
#define MCP9700_ADC_INTERFACE_HPP

#include <concepts>
#include <cstdint>

#include "mcp9700_detail.hpp"

namespace mcp9700 {

/// Most current the output may source or sink, microamps (I_OUT, DS20001942L
/// p.4): the ADC input, including its sample-and-hold charge, must stay below
/// it on average.
inline constexpr uint32_t kMaxOutputCurrentUa = 100U;
/// Capacitive load the family is characterised and production tested with,
/// picofarads (p.4, note 4).
inline constexpr uint32_t kMaxLoadCapacitancePf = 1000U;
/// Typical output impedance, ohms, at 100 uA and 500 Hz (Z_OUT, p.4).
inline constexpr uint32_t kOutputImpedanceOhm = 20U;
/// Typical turn-on time, microseconds (t_ON, p.4); the datasheet gives no
/// maximum.
inline constexpr uint32_t kPowerOnTypicalUs = 800U;

/** @brief Control pins the port may drive through the optional `GpioSet()`
 * hook. */
enum class CtrlPin : uint8_t {
  Enable = 0, ///< Supply of the sensor from an MCU pin (Section 4.2); ACTIVE
              ///< means powered.
};

/** @brief Logical level of a control pin; the port maps it to the physical
 * polarity. */
enum class GpioSignal : uint8_t {
  INACTIVE = 0, ///< The pin function is deasserted.
  ACTIVE = 1,   ///< The pin function is asserted.
};

/**
 * @brief CRTP base of the platform ADC port.
 *
 * The base declares no hook itself (a forwarding hook would turn a missing
 * override into infinite recursion); the driver checks your class with the
 * `AdcPort` concept.
 *
 * @tparam Derived Your port class.
 */
template <typename Derived>
class AdcInterface {
public:
  /** @brief Ports are not copied: the driver holds a reference. */
  AdcInterface(const AdcInterface&) = delete;
  /** @brief Ports are not copied: the driver holds a reference. */
  AdcInterface& operator=(const AdcInterface&) = delete;

protected:
  /** @brief Only the derived port is constructed. */
  AdcInterface() noexcept = default;
  /** @brief Destroyed through the derived port only. */
  ~AdcInterface() noexcept = default;
};

/**
 * @brief Satisfied by a class that derives from `AdcInterface<Port>` and
 * implements the required hooks with exactly the types above, `noexcept` and
 * returning `bool` (handbook KB-CPP20-006).
 * @tparam Port The port class.
 */
template <typename Port>
concept AdcPort = std::derived_from<Port, AdcInterface<Port>> && requires {
  detail::ExactHook<Port, bool() noexcept>::Take(&Port::EnsureInitialized);
  detail::ExactHook<Port, bool(uint8_t, uint32_t&) noexcept>::Take(&Port::ReadVoltageMv);
};

} // namespace mcp9700

#endif // MCP9700_ADC_INTERFACE_HPP
