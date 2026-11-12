/**
 * @file mcp9700.ipp
 * @brief MCP970x driver implementation, included by `mcp9700.hpp`.
 * @copyright Copyright (c) 2026 The driver-mcp9700 authors.
 * SPDX-License-Identifier: MIT
 */
#ifndef MCP9700_IPP
#define MCP9700_IPP

namespace mcp9700 {

namespace detail {

inline Result<Measurement> Evaluate(const Config& config, const VariantSpec& spec,
                                    const Samples& samples) noexcept {
  // Clipped ADC: a conversion within one step of the declared full scale, or
  // above it (p.9: the sensor's own output never reaches it).
  if ((static_cast<uint64_t>(samples.max) + AdcLsbMv(config)) >= config.adc_full_scale_mv) {
    return Result<Measurement>::Failure(Error::Saturated);
  }
  const int64_t num = config.divider_num;
  const int64_t den = config.divider_den;
  const int64_t count = config.samples;
  const int64_t spread = RoundDivAway(
      (static_cast<int64_t>(samples.max) - static_cast<int64_t>(samples.min)) * num, den);
  if (spread > static_cast<int64_t>(config.max_spread_mv)) {
    return Result<Measurement>::Failure(Error::Unstable);
  }
  // One integer expression, no intermediate rounding: Equation 4-1 solved for
  // the temperature.
  const int64_t scaled = static_cast<int64_t>(samples.sum) * num;
  const int64_t divisor = count * den;
  const int64_t output_mv = RoundDivAway(scaled, divisor);
  const int64_t offset = count * static_cast<int64_t>(spec.v0_mv) * den;
  const int64_t raw_mc =
      RoundDivAway((scaled - offset) * kPerMega, divisor * static_cast<int64_t>(spec.tc_uv_per_c));
  const bool high = config.range == TemperatureClass::High;
  const int64_t top = high ? spec.max_high_mc : spec.max_ext_mc;
  if (raw_mc < (static_cast<int64_t>(spec.min_mc) - config.range_margin_mc)) {
    return Result<Measurement>::Failure(Error::OutputLow);
  }
  if (raw_mc > (top + config.range_margin_mc)) {
    return Result<Measurement>::Failure(Error::OutputHigh);
  }
  Measurement m{};
  m.temperature_mc = static_cast<int32_t>(raw_mc);
  m.output_mv = static_cast<uint32_t>(output_mv);
  m.spread_mv = static_cast<uint32_t>(spread);
  return Result<Measurement>::Success(m);
}

} // namespace detail

template <AdcPort Port>
Result<void> MCP9700<Port>::EnsureInitialized() noexcept {
  if (initialized_) {
    return Result<void>::Success();
  }
  if (auto r = initialize(); !r) {
    return r;
  }
  initialized_ = true;
  return Result<void>::Success();
}

template <AdcPort Port>
Result<void> MCP9700<Port>::initialize() noexcept {
  const auto spec = ResolveSpec(config_);
  if (!spec) {
    return Result<void>::Failure(Error::InvalidArgument);
  }
  spec_ = spec.value;
  if (config_.variant == Variant::Custom) {
    detail::Log(port_, LogLevel::Warning,
                "mcp9700: custom sensor, coefficients are the caller's responsibility");
  }
  if (!port_.EnsureInitialized()) {
    detail::Log(port_, LogLevel::Error, "mcp9700: the port could not bring the ADC up");
    return Result<void>::Failure(Error::Bus);
  }
  if constexpr (detail::HasGpioSet<Port>) {
    if (!port_.GpioSet(CtrlPin::Enable, GpioSignal::ACTIVE)) {
      // A failed write may still have energised the sensor: switch it off, best effort.
      [[maybe_unused]] const auto off = switchOff();
      return Result<void>::Failure(Error::Bus);
    }
  }
  // t_ON is 800 us typical, no maximum (p.4): the wait is a knob, and it is
  // never skipped.
  detail::DelayUs(port_, config_.power_on_settle_us);
  const auto first = readValidated();
  if (!first) {
    return failStartup(first.error);
  }
  return Result<void>::Success();
}

template <AdcPort Port>
Result<void> MCP9700<Port>::failStartup(Error reason) noexcept {
  // Best effort: switchOff() logs a failure, and the start-up error below is what the caller needs.
  [[maybe_unused]] const auto off = switchOff();
  if (reason == Error::Bus) {
    return Result<void>::Failure(Error::Bus);
  }
  detail::Log(port_, LogLevel::Error, detail::StartupReason(reason));
  return Result<void>::Failure(Error::DeviceIdMismatch);
}

template <AdcPort Port>
Result<void> MCP9700<Port>::switchOff() noexcept {
  if constexpr (detail::HasGpioSet<Port>) {
    if (!port_.GpioSet(CtrlPin::Enable, GpioSignal::INACTIVE)) {
      detail::Log(port_, LogLevel::Warning, "mcp9700: could not switch the sensor off");
      return Result<void>::Failure(Error::Bus);
    }
  }
  return Result<void>::Success();
}

template <AdcPort Port>
Result<void> MCP9700<Port>::Deinitialize() noexcept {
  if (!initialized_) {
    return Result<void>::Success();
  }
  initialized_ = false;
  last_raw_mv_ = 0U;
  return switchOff();
}

template <AdcPort Port>
Result<void> MCP9700<Port>::Sleep() noexcept {
  if (!initialized_) {
    return Result<void>::Failure(Error::NotInitialized);
  }
  if constexpr (!detail::HasGpioSet<Port>) {
    return Result<void>::Failure(Error::Unsupported);
  } else {
    return Deinitialize();
  }
}

template <AdcPort Port>
Result<Measurement> MCP9700<Port>::ReadMeasurement() noexcept {
  if (!initialized_) {
    return Result<Measurement>::Failure(Error::NotInitialized);
  }
  auto reading = readValidated();
  if (!reading) {
    return reading;
  }
  // The calibration is added after the window check: the result may sit up to
  // 20 C beyond an edge.
  reading.value.temperature_mc += config_.offset_cal_mc;
  return reading;
}

template <AdcPort Port>
Result<int32_t> MCP9700<Port>::ReadTemperatureMilliC() noexcept {
  const auto reading = ReadMeasurement();
  if (!reading) {
    return Result<int32_t>::Failure(reading.error);
  }
  return Result<int32_t>::Success(reading.value.temperature_mc);
}

template <AdcPort Port>
Result<uint32_t> MCP9700<Port>::ReadOutputMv() noexcept {
  const auto reading = ReadMeasurement();
  if (!reading) {
    return Result<uint32_t>::Failure(reading.error);
  }
  return Result<uint32_t>::Success(reading.value.output_mv);
}

template <AdcPort Port>
Result<uint32_t> MCP9700<Port>::ReadRawPortMv() noexcept {
  if (!initialized_) {
    return Result<uint32_t>::Failure(Error::NotInitialized);
  }
  return readOnce();
}

template <AdcPort Port>
uint32_t MCP9700<Port>::ResolutionMilliC() const noexcept {
  const auto spec = ResolveSpec(config_);
  if (!spec) {
    return 0U;
  }
  constexpr uint64_t kMicroPerMilli = 1000ULL;
  const uint64_t step_uv = detail::CeilDiv(
      static_cast<uint64_t>(config_.adc_full_scale_mv) * kMicroPerMilli * config_.divider_num,
      static_cast<uint64_t>(config_.divider_den) * detail::AdcCodes(config_));
  return static_cast<uint32_t>(detail::CeilDiv(step_uv * kMicroPerMilli, spec.value.tc_uv_per_c));
}

template <AdcPort Port>
Result<uint32_t> MCP9700<Port>::readOnce() noexcept {
  uint32_t mv = 0U;
  if (!port_.ReadVoltageMv(config_.channel, mv)) {
    last_raw_mv_ = 0U; // a failed conversion leaves no stale value behind
    return Result<uint32_t>::Failure(Error::Bus);
  }
  last_raw_mv_ = mv;
  return Result<uint32_t>::Success(mv);
}

template <AdcPort Port>
Result<detail::Samples> MCP9700<Port>::collect() noexcept {
  const uint32_t discard = config_.discard_conversions;
  const uint32_t total = discard + static_cast<uint32_t>(config_.samples);
  detail::Samples samples{};
  for (uint32_t i = 0U; i < total; ++i) {
    if ((i > 0U) && (config_.sample_interval_us > 0U)) {
      detail::DelayUs(port_, config_.sample_interval_us);
    }
    const auto mv = readOnce();
    if (!mv) {
      return Result<detail::Samples>::Failure(Error::Bus);
    }
    if (i >= discard) {
      samples.sum += mv.value;
      samples.min = (mv.value < samples.min) ? mv.value : samples.min;
      samples.max = (mv.value > samples.max) ? mv.value : samples.max;
    }
  }
  return Result<detail::Samples>::Success(samples);
}

template <AdcPort Port>
Result<Measurement> MCP9700<Port>::readValidated() noexcept {
  const auto samples = collect();
  if (!samples) {
    return Result<Measurement>::Failure(samples.error);
  }
  return detail::Evaluate(config_, spec_, samples.value);
}

} // namespace mcp9700

#endif // MCP9700_IPP
