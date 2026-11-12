/**
 * @file mock_mcp9700.hpp
 * @brief A model of an MCP970x analog temperature sensor and the ADC in front
 * of it, plus the mock ports.
 *
 * The model is written from the datasheet (DS20001942L) on its own, in floating
 * point: it includes none of the driver's headers and carries its own copy of
 * the datasheet rows (namespace `chip`), so a wrong constant in the driver's
 * variant table cannot hide behind the same wrong constant here.
 *
 * - `Sensor`: the output V = V0 + TC x T plus an optional bounded nonlinearity
 * (0 at +25 C, up to
 *   +-0.5 C at 0 and +70 C, p.4), an accuracy offset that places the part
 * anywhere in its band, the line regulation (0.1 C/V away from the tested
 * supply), and the turn-on time: until `turn_on_us` of modelled time have
 * passed since power-on the output is `unpowered_mv`. Time advances only
 *   through `DelayUs()` and a fixed conversion time, so a driver that does not
 * wait reads a low output.
 * - `Adc`: full scale, resolution, offset and gain error, quantisation to codes
 * and then to whole millivolts, clipping at the top code, deterministic noise,
 * a divider ahead of the pin and a channel multiplexer with a hold capacitor
 * (the first conversion after a channel switch mixes in the previous channel).
 * - Faults: stuck low, floating high, a drifting floating input, a floating
 * input that holds a plausible value, spikes, a clip below the declared full
 * scale, a failing conversion, a failing ADC bring-up or pin write, a supply
 * that drops mid-run.
 *
 * | Port | Hooks |
 * | --- | --- |
 * | `MinimalPort` | the required ones plus `DelayUs()` (needed by every wait) |
 * | `FullPort` | also `GpioSet()` (the sensor's supply pin) and `Log()` |
 */
#ifndef MCP9700_MOCK_MCP9700_HPP
#define MCP9700_MOCK_MCP9700_HPP

#include <cmath>
#include <cstddef>
#include <cstdint>

#include "mcp9700_adc_interface.hpp"
#include "mcp9700_types.hpp"

namespace mcp9700_test {

/// The datasheet rows, independent of the driver's table.
namespace chip {

/// One row: transfer function, ranges, supply and tested supply (DS20001942L
/// p.3, p.4).
struct Row {
  const char* name;
  double v0_mv;        ///< Output at 0 C, typical.
  double tc_mv_per_c;  ///< Temperature coefficient, typical.
  double min_c;        ///< Lowest specified temperature.
  double max_c;        ///< Highest specified temperature, Extended.
  double max_high_c;   ///< Highest, High option; equal to `max_c` when there is
                       ///< none.
  bool has_high;       ///< The High option exists.
  double vdd_min_v;    ///< Lowest supply.
  double vdd_max_v;    ///< Highest supply.
  double tested_vdd_v; ///< Supply at which the accuracy is tested.
};

inline constexpr std::size_t kRowCount = 5;

/// MCP9700, MCP9700A, MCP9700B, MCP9701, MCP9701A, in `Variant` order.
inline constexpr Row kRows[kRowCount] = {
    {"MCP9700", 500.0, 10.0, -40.0, 125.0, 150.0, true, 2.3, 5.5, 3.3},
    {"MCP9700A", 500.0, 10.0, -40.0, 125.0, 125.0, false, 2.3, 5.5, 3.3},
    {"MCP9700B", 500.0, 10.0, -40.0, 125.0, 150.0, true, 2.3, 5.5, 3.3},
    {"MCP9701", 400.0, 19.5, -10.0, 125.0, 125.0, false, 3.1, 5.5, 5.0},
    {"MCP9701A", 400.0, 19.5, -10.0, 125.0, 125.0, false, 3.1, 5.5, 5.0},
};

/// A synthetic first-order part for `Variant::Custom` tests.
struct Synthetic {
  const char* name;
  uint32_t v0_mv;
  uint32_t tc_uv_per_c;
  int32_t min_mc;
  int32_t max_mc;
};

inline constexpr std::size_t kSyntheticCount = 5;

/// Synthetic parts (values from no datasheet: they exist to exercise the
/// arithmetic).
inline constexpr Synthetic kSynthetics[kSyntheticCount] = {
    {"0 mV, 10 mV/C", 0, 10'000, 0, 150'000},
    {"750 mV, 10 mV/C", 750, 10'000, -40'000, 125'000},
    {"1100 mV, 50 mV/C", 1100, 50'000, -20'000, 60'000},
    {"2000 mV, 1 mV/C", 2000, 1000, -50'000, 100'000},
    {"0 mV, 100 mV/C", 0, 100'000, 0, 30'000},
};

} // namespace chip

/// What goes wrong on the line between the sensor and the converter.
enum class Fault : uint8_t {
  None = 0,
  StuckLow,      ///< The pin reads 0 V.
  FloatHigh,     ///< The pin reads the ADC's full scale.
  FloatingDrift, ///< A random walk with a step above any sensible spread
                 ///< threshold.
  FloatingHold,  ///< A stable, plausible value (a hold capacitor on an unwired
                 ///< channel).
  Spike,         ///< One conversion in `spike_every` is an outlier.
};

/// A deterministic pseudo-random source (xorshift64), so every run is the same.
class Rng {
public:
  /// A number in [-1, 1].
  double Next() noexcept {
    state_ ^= state_ << 13U;
    state_ ^= state_ >> 7U;
    state_ ^= state_ << 17U;
    return (static_cast<double>(state_ % 2'000'001ULL) / 1'000'000.0) - 1.0;
  }

private:
  uint64_t state_ = 0x9E3779B97F4A7C15ULL;
};

/// The sensor, the ADC in front of it, fault injection and counters.
struct MockBusState {
  // Fields are ordered by size (8, 4, then 1 byte) so the struct carries no
  // padding.
  // --- the sensor ---
  double v0_mv = 500.0;          ///< The part's V0 (set by `UseRow`).
  double tc_mv_per_c = 10.0;     ///< The part's TC.
  double nonlinearity_c = 0.0;   ///< Amplitude of the bowl-shaped nonlinearity, C (0 or 0.5).
  double vdd_mv = 3300.0;        ///< The supply voltage.
  double tested_vdd_mv = 3300.0; ///< The supply the accuracy is specified at.
  double turn_on_us = 800.0;     ///< t_ON, typical (p.4).
  double unpowered_mv = 0.0;     ///< The output before `turn_on_us` and while off.
  int64_t time_us = 0;           ///< Modelled time.
  int64_t powered_at_us = -1'000'000'000; ///< When the supply last came on.
  // --- the ADC and the wiring ---
  double unwired_mv = 0.0;      ///< What an unwired channel reads (it is pulled down).
  double offset_error_mv = 0.0; ///< ADC offset error at the pin.
  double gain_error_ppm = 0.0;  ///< ADC gain error.
  double noise_mv = 0.0;        ///< Peak noise per conversion at the pin.
  double clip_mv = -1.0;        ///< The ADC clips here when it is >= 0 (below its full scale).
  double hold_ratio = 0.0;      ///< Crosstalk after a channel switch (0 to 1).
  double divider_gain = 1.0;    ///< Pin voltage = sensor voltage x this (an external divider).
  // --- faults ---
  double drift_step_mv = 200.0; ///< `FloatingDrift` step.
  double hold_level_mv = 800.0; ///< `FloatingHold` level.
  double spike_mv = 2500.0;     ///< `Spike` value.
  // --- counters ---
  uint64_t delayed_us = 0;  ///< Sum of DelayUs() arguments.
  double last_pin_mv = 0.0; ///< The true pin voltage of the last conversion.
  double drift_mv = 800.0;  ///< Drifting level.
  Rng rng{};
  // --- 4-byte fields ---
  int32_t temperature_mc = 25'000; ///< The true temperature the die sees.
  int32_t accuracy_offset_mc = 0;  ///< Where in its accuracy band the part sits.
  uint32_t full_scale_mv = 3300;   ///< The ADC's real full scale.
  int conversion_us = 10;          ///< Modelled time per conversion.
  int spike_every = 0;             ///< `Spike`: every n-th conversion (0 off).
  int fail_transfer = -1;          ///< Fail the conversion with this index (0-based); -1 never.
  int transfers = 0;               ///< Conversions so far.
  int bus_up_calls = 0;            ///< Port EnsureInitialized() calls so far.
  uint32_t max_delay_us = 0;       ///< Largest single DelayUs() argument.
  int delay_calls = 0;             ///< DelayUs() calls.
  int last_channel = -1;           ///< The channel of the last conversion.
  // --- 1-byte fields ---
  Fault fault = Fault::None;
  uint8_t wired_channel = 0;        ///< The channel the output is wired to.
  uint8_t bits = 12;                ///< The ADC's real resolution.
  bool use_line_regulation = false; ///< Apply 0.1 C/V away from `tested_vdd_mv`.
  bool powered = true;              ///< The supply pin level (a port without GpioSet is always on).
  bool bus_up_ok = true;            ///< The port's EnsureInitialized() result.

  /// Set the part from a datasheet row.
  void UseRow(const chip::Row& row) noexcept {
    v0_mv = row.v0_mv;
    tc_mv_per_c = row.tc_mv_per_c;
    tested_vdd_mv = row.tested_vdd_v * 1000.0;
    vdd_mv = tested_vdd_mv;
  }
  /// Set the part from a synthetic spec.
  void UseSynthetic(const chip::Synthetic& s) noexcept {
    v0_mv = static_cast<double>(s.v0_mv);
    tc_mv_per_c = static_cast<double>(s.tc_uv_per_c) / 1000.0;
  }

  /// The sensor's supply pin changes level (the port's `GpioSet`).
  void SetPower(bool on) noexcept {
    if (on && !powered) {
      powered_at_us = time_us;
    }
    powered = on;
  }

  /// Modelled time passes.
  void Advance(uint32_t us) noexcept {
    time_us += static_cast<int64_t>(us);
  }

  /// The nonlinearity at `t_c`: 0 at 25 C, `nonlinearity_c` at 0 and 70 C, bowl
  /// shaped.
  [[nodiscard]] double Nonlinearity(double t_c) const noexcept {
    const double span = (t_c < 25.0) ? 25.0 : 45.0;
    const double x = (t_c - 25.0) / span;
    return nonlinearity_c * x * x;
  }

  /// The sensor's output voltage now, millivolts at VOUT.
  [[nodiscard]] double SensorMv() const noexcept {
    const bool ready = powered && ((time_us - powered_at_us) >= static_cast<int64_t>(turn_on_us));
    if (!ready) {
      return unpowered_mv;
    }
    double t_c =
        (static_cast<double>(temperature_mc) + static_cast<double>(accuracy_offset_mc)) / 1000.0;
    t_c += Nonlinearity(t_c);
    if (use_line_regulation) {
      t_c += 0.1 * ((vdd_mv - tested_vdd_mv) / 1000.0);
    }
    const double v = v0_mv + (tc_mv_per_c * t_c);
    return (v > 0.0) ? v : 0.0;
  }

  /// Quantise a pin voltage the way the ADC does and report whole millivolts.
  [[nodiscard]] uint32_t Quantise(double pin_mv) const noexcept {
    const double codes = std::ldexp(1.0, static_cast<int>(bits));
    const auto fs = static_cast<double>(full_scale_mv);
    double v = (pin_mv * (1.0 + (gain_error_ppm / 1e6))) + offset_error_mv;
    v = (v < 0.0) ? 0.0 : v;
    double code = std::floor((v * codes / fs) + 0.5);
    code = (code > (codes - 1.0)) ? (codes - 1.0) : code;
    return static_cast<uint32_t>(std::floor((code * fs / codes) + 0.5));
  }

  /// One conversion of `channel`.
  bool Convert(uint8_t channel, uint32_t& mv) noexcept {
    const int index = transfers++;
    time_us += conversion_us;
    if (index == fail_transfer) {
      return false;
    }
    double pin = (channel == wired_channel) ? (SensorMv() * divider_gain) : unwired_mv;
    pin = faulted(pin, channel);
    const double truth = pin;
    if ((clip_mv >= 0.0) && (pin > clip_mv)) {
      pin = clip_mv;
    }
    if ((last_channel >= 0) && (static_cast<uint8_t>(last_channel) != channel)) {
      pin = (pin * (1.0 - hold_ratio)) + (last_pin_mv * hold_ratio);
    }
    pin += noise_mv * rng.Next();
    last_channel = static_cast<int>(channel);
    last_pin_mv = truth;
    mv = Quantise(pin);
    return true;
  }

private:
  [[nodiscard]] double faulted(double pin, uint8_t channel) noexcept {
    switch (fault) {
    case Fault::StuckLow:
      return 0.0;
    case Fault::FloatHigh:
      return static_cast<double>(full_scale_mv);
    case Fault::FloatingDrift:
      drift_mv += drift_step_mv * rng.Next();
      drift_mv = (drift_mv < 0.0) ? 0.0 : drift_mv;
      return drift_mv;
    case Fault::FloatingHold:
      return (channel == wired_channel) ? pin : hold_level_mv;
    case Fault::Spike:
      return ((spike_every > 0) && ((transfers % spike_every) == 0)) ? spike_mv : pin;
    case Fault::None:
    default:
      break;
    }
    return pin;
  }
};

/// The required ADC hooks over a MockBusState. Derive `class P : public
/// MockPortBase<P>`.
template <typename Self>
class MockPortBase : public mcp9700::AdcInterface<Self> {
public:
  MockBusState state;

  bool EnsureInitialized() noexcept {
    state.bus_up_calls++;
    return state.bus_up_ok;
  }
  bool ReadVoltageMv(uint8_t channel, uint32_t& voltage_mv) noexcept {
    return state.Convert(channel, voltage_mv);
  }
};

/// Required hooks plus the delay.
class MinimalPort : public MockPortBase<MinimalPort> {
public:
  void DelayUs(uint32_t us) noexcept {
    state.Advance(us);
    state.delayed_us += us;
    state.delay_calls++;
    state.max_delay_us = (us > state.max_delay_us) ? us : state.max_delay_us;
  }
};

/// Every optional hook, with counters. The supply pin idles INACTIVE
/// (unpowered) before the driver enables it (KB-HW-018).
class FullPort : public MockPortBase<FullPort> {
public:
  int log_lines = 0;              ///< Log() calls.
  int warnings = 0;               ///< Log() calls at Warning level.
  int errors = 0;                 ///< Log() calls at Error level.
  const char* last_log = nullptr; ///< The last message.
  bool gpio_ok = true;            ///< GpioSet() result.
  int gpio_fail_at = -1;          ///< Fail the GpioSet() call with this index (0-based).
  int gpio_calls = 0;             ///< GpioSet() calls.
  int gpio_active = 0;            ///< GpioSet(ACTIVE) calls that succeeded.
  int gpio_inactive = 0;          ///< GpioSet(INACTIVE) calls that succeeded.
  bool last_level_active = false; ///< The last level written.

  FullPort() noexcept {
    state.powered = false;
  }

  void DelayUs(uint32_t us) noexcept {
    state.Advance(us);
    state.delayed_us += us;
    state.delay_calls++;
    state.max_delay_us = (us > state.max_delay_us) ? us : state.max_delay_us;
  }
  bool GpioSet(mcp9700::CtrlPin pin, mcp9700::GpioSignal signal) noexcept {
    const int index = gpio_calls++;
    if (!gpio_ok || (index == gpio_fail_at) || (pin != mcp9700::CtrlPin::Enable)) {
      return false;
    }
    const bool active = signal == mcp9700::GpioSignal::ACTIVE;
    state.SetPower(active);
    last_level_active = active;
    (active ? gpio_active : gpio_inactive)++;
    return true;
  }
  void Log(mcp9700::LogLevel level, const char* message) noexcept {
    log_lines++;
    warnings += (level == mcp9700::LogLevel::Warning) ? 1 : 0;
    errors += (level == mcp9700::LogLevel::Error) ? 1 : 0;
    last_log = message;
  }
};

} // namespace mcp9700_test

#endif // MCP9700_MOCK_MCP9700_HPP
