/**
 * @file fuzz_mcp9700.cpp
 * @brief Fuzz target: the driver over an ADC whose readings the fuzzer
 * controls.
 *
 * What the driver reads is a voltage from the port's ADC, so the reading is the
 * untrusted input. The fuzzer's bytes choose a configuration (sometimes
 * invalid: any variant including `Custom` with random fields, a named variant
 * with a stray `custom` field, an out-of-enumeration variant), then up to 64
 * operations. On every conversion the bytes also choose: pass the model's
 * output, fail, or return any 32-bit value. The port records everything it did
 * in the last call, so the target has an oracle that does not trust the driver:
 * tests/reference_mcp9700.hpp keeps its own copy of the datasheet rows, its own
 * implementation of the `Custom` rules and 128-bit rational arithmetic.
 *
 * Invariants checked after every call (docs/testing.md lists them as F1 to
 * F15); each is one `Check`, and a broken one aborts naming the line:
 *
 * - F1 a failed call returns an error this driver may return, and a zero value;
 * - F2 before a successful start (and after `Deinitialize`/`Sleep`) every call
 * but `EnsureInitialized` and `Deinitialize` is `NotInitialized` with zero
 * traffic;
 * - F3 `InvalidArgument` means zero traffic and only happens when the oracle
 * rejects the config;
 * - F4 a successful reading used exactly `discard + samples` conversions and
 * the right waits;
 * - F5 the error and the value equal the oracle's, recomputed from the recorded
 * conversions;
 * - F6 a success holds no conversion near full scale and no spread above the
 * limit;
 * - F7 `ReadRawPortMv` sends one conversion and returns what the port gave; the
 * last-value getter follows every conversion;
 * - F8 `Bus` exactly when a conversion, the ADC bring-up or the supply write
 * failed in that call;
 * - F9 after a successful stop the supply was driven INACTIVE, a failed start
 * switches it off, and only `EnsureInitialized` ever drives it ACTIVE;
 * - F10 a failed start leaves the driver uninitialised; a second start is free;
 * - F11 the waits of a successful start total at least the settle time, none is
 * longer than the longest knob;
 * - F12 every log line is non-empty plain text;
 * - F14/F15 named-variant, `Custom` and `High` rules agree with the oracle, and
 * a `Custom` start logs its one warning;
 * - F13 no undefined behaviour for any input (UBSan with
 * `-fno-sanitize-recover=all`).
 *
 * Built two ways (tests/fuzz/CMakeLists.txt): with libFuzzer (Clang,
 * `-DMCP9700_BUILD_FUZZERS=ON`), and as a plain executable that replays the
 * corpus in tests/fuzz/corpus/ as a CTest on any compiler.
 */
#include <bit>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <utility>

#include "mcp9700.hpp"
#include "mock_mcp9700.hpp"
#include "reference_mcp9700.hpp"

namespace {

using mcp9700::Error;

/// The fuzzer's bytes, read front to back; zeros once they run out.
class Input {
public:
  Input(const uint8_t* data, std::size_t size) noexcept : data_(data), size_(size) {}

  [[nodiscard]] bool Empty() const noexcept {
    return pos_ >= size_;
  }
  uint8_t Byte() noexcept {
    return pos_ < size_ ? data_[pos_++] : uint8_t{0};
  }
  uint16_t Word() noexcept {
    const auto high = Byte();
    return static_cast<uint16_t>((static_cast<uint32_t>(high) << 8U) | Byte());
  }
  uint32_t Dword() noexcept {
    const uint32_t high = Word();
    return (high << 16U) | Word();
  }

private:
  const uint8_t* data_;
  std::size_t size_;
  std::size_t pos_ = 0;
};

/// A broken promise: name the line and abort, so libFuzzer saves the input as a
/// crash.
void Check(bool promise, int line = __builtin_LINE()) noexcept {
  if (!promise) {
    (void)std::fprintf(stderr, "fuzz_mcp9700: promise broken at line %d\n", line);
    std::abort();
  }
}

constexpr uint8_t kFail = 0xFF;    ///< The conversion (or pin write) fails.
constexpr uint8_t kReplace = 0xFD; ///< The conversion returns any 32-bit value.
constexpr std::size_t kMaxRecorded = 4 + 64;

/// The port: the sensor model of the host tests behind an ADC the fuzzer
/// controls, recording what the driver did in the current call.
class FuzzPort : public mcp9700::AdcInterface<FuzzPort> {
public:
  explicit FuzzPort(Input& in) noexcept : in_(in) {
    sensor.powered = false;
  }

  bool EnsureInitialized() noexcept {
    bring_ups++;
    return bus_up_ok;
  }

  bool ReadVoltageMv(uint8_t channel, uint32_t& voltage_mv) noexcept {
    const uint8_t action = in_.Byte();
    conversions++;
    if (action == kFail) {
      conversion_failed = true;
      return false;
    }
    uint32_t value = 0;
    if (action == kReplace) {
      value = in_.Dword();
      sensor.Advance(10);
    } else {
      (void)sensor.Convert(channel, value);
    }
    if (recorded_count < kMaxRecorded) {
      recorded[recorded_count++] = value;
    }
    last_value = value;
    voltage_mv = value;
    return true;
  }

  void DelayUs(uint32_t us) noexcept {
    sensor.Advance(us);
    waits++;
    waited_us += us;
    longest_wait = (us > longest_wait) ? us : longest_wait;
  }

  bool GpioSet(mcp9700::CtrlPin pin, mcp9700::GpioSignal signal) noexcept {
    Check(pin == mcp9700::CtrlPin::Enable);
    const bool active = signal == mcp9700::GpioSignal::ACTIVE;
    gpio_writes++;
    if (in_.Byte() == kFail) {
      (active ? active_write_failed : inactive_write_failed) = true;
      return false;
    }
    sensor.SetPower(active);
    last_level_known = true;
    last_level_active = active;
    active_writes += active ? 1 : 0;
    return true;
  }

  void Log(mcp9700::LogLevel level, const char* message) noexcept {
    (void)level;
    Check((message != nullptr) && (message[0] != '\0'));
    logs++;
    warnings += (level == mcp9700::LogLevel::Warning) ? 1 : 0;
  }

  /// Forget the per-call record.
  void BeginCall() noexcept {
    conversions = 0;
    recorded_count = 0;
    bring_ups = 0;
    gpio_writes = 0;
    active_writes = 0;
    waits = 0;
    waited_us = 0;
    longest_wait = 0;
    logs = 0;
    warnings = 0;
    conversion_failed = false;
    active_write_failed = false;
    inactive_write_failed = false;
  }

  mcp9700_test::MockBusState sensor;
  bool bus_up_ok = true;
  int conversions = 0;
  int bring_ups = 0;
  int gpio_writes = 0;
  int active_writes = 0;
  int waits = 0;
  uint64_t waited_us = 0;
  uint32_t longest_wait = 0;
  int logs = 0;
  int warnings = 0;
  bool conversion_failed = false;
  bool active_write_failed = false;
  bool inactive_write_failed = false;
  bool last_level_known = false;
  bool last_level_active = false;
  uint32_t last_value = 0;
  uint32_t recorded[kMaxRecorded]{};
  std::size_t recorded_count = 0;

private:
  Input& in_;
};

using Driver = mcp9700::MCP9700<FuzzPort>;

/// The errors this driver may return; `Timeout`, `Crc`, `Busy` and
/// `DeviceFault` never.
bool KnownError(Error e) noexcept {
  if ((e == Error::Timeout) || (e == Error::Crc) || (e == Error::Busy) ||
      (e == Error::DeviceFault)) {
    return false;
  }
  const auto v = static_cast<uint8_t>(e);
  return (v <= 9U) || ((v >= 16U) && (v <= 19U));
}

/// A configuration from the fuzzer's bytes; sometimes invalid on purpose.
mcp9700::Config FuzzConfig(Input& in) noexcept {
  mcp9700::Config c{};
  const uint8_t flags = in.Byte();
  c.channel = static_cast<uint8_t>((flags >> 6U) % 3U);
  c.variant = std::bit_cast<mcp9700::Variant>(
      static_cast<uint8_t>(in.Byte() % 7U)); // 6 is not an enumerator
  c.range = std::bit_cast<mcp9700::TemperatureClass>(static_cast<uint8_t>(
      (in.Byte() % 5U) == 4U ? 2U : (in.Byte() & 1U))); // 2 is not an enumerator
  const uint8_t custom_mode = in.Byte();
  if (c.variant == mcp9700::Variant::Custom) {
    if ((custom_mode & 1U) != 0U) { // a sane declaration, perturbed
      c.custom = {750, 10'000, -40'000, 125'000, -2000, 3000};
      c.custom.v0_mv += (custom_mode & 2U) != 0U ? in.Word() : 0U;
      c.custom.tc_uv_per_c += (custom_mode & 4U) != 0U ? in.Word() : 0U;
      c.custom.min_mc -= (custom_mode & 8U) != 0U ? in.Word() : 0;
      c.custom.max_mc += (custom_mode & 16U) != 0U ? in.Word() : 0;
      c.custom.accuracy_max_mc =
          (custom_mode & 32U) != 0U ? static_cast<int32_t>(in.Word()) - 8000 : 3000;
    } else { // random fields
      c.custom.v0_mv = in.Word();
      c.custom.tc_uv_per_c = in.Dword() % 120'000U;
      c.custom.min_mc = static_cast<int32_t>(in.Dword() % 500'000U) - 250'000;
      c.custom.max_mc = static_cast<int32_t>(in.Dword() % 500'000U) - 250'000;
      c.custom.accuracy_min_mc = -static_cast<int32_t>(in.Word() % 25'000U);
      c.custom.accuracy_max_mc = static_cast<int32_t>(in.Word() % 25'000U);
    }
  } else if ((custom_mode & 0x80U) != 0U) { // a named variant with a stray field
    c.custom.tc_uv_per_c = in.Word();
    c.custom.accuracy_max_mc = static_cast<int32_t>(custom_mode & 3U);
  }
  if ((flags & 0x01U) != 0U) {
    c.adc_full_scale_mv = 50U + (in.Word() % 10'300U);
    c.adc_resolution_bits = static_cast<uint8_t>(7U + (in.Byte() % 11U));
    c.divider_num = (in.Byte() & 1U) != 0U ? in.Word() : static_cast<uint16_t>(in.Byte() % 4U);
    c.divider_den = (in.Byte() & 1U) != 0U ? in.Word() : static_cast<uint16_t>(in.Byte() % 4U);
    c.offset_cal_mc = static_cast<int32_t>(in.Word() % 45'000U) - 22'500;
    c.range_margin_mc = static_cast<int32_t>(in.Word() % 22'000U) - 500;
    c.samples = static_cast<uint16_t>(in.Byte() % 70U);
    c.sample_interval_us = (in.Byte() & 1U) != 0U ? in.Word() : (in.Byte() % 40U);
    c.discard_conversions = static_cast<uint8_t>(in.Byte() % 7U);
    c.max_spread_mv = in.Word() % 1200U;
    c.power_on_settle_us = (in.Byte() & 1U) != 0U ? in.Dword() % 1'200'000U : (in.Byte() * 20U);
  }
  return c;
}

/// Change the model between two calls; its spec is chosen independently of the
/// configuration.
void FuzzModel(FuzzPort& port, Input& in) noexcept {
  auto& s = port.sensor;
  switch (in.Byte() % 8U) {
  case 0:
    s.temperature_mc = (static_cast<int32_t>(in.Byte()) - 100) * 1000; // -100 to +155 C
    break;
  case 1:
    s.UseRow(mcp9700_test::chip::kRows[in.Byte() % mcp9700_test::chip::kRowCount]);
    break;
  case 2:
    s.UseSynthetic(
        mcp9700_test::chip::kSynthetics[in.Byte() % mcp9700_test::chip::kSyntheticCount]);
    break;
  case 3:
    s.wired_channel = static_cast<uint8_t>(in.Byte() % 3U);
    break;
  case 4:
    s.SetPower((in.Byte() & 1U) != 0U);
    break;
  case 5:
    s.fault = static_cast<mcp9700_test::Fault>(in.Byte() % 6U);
    s.spike_every = static_cast<int>(in.Byte() % 5U);
    break;
  case 6:
    port.bus_up_ok = (in.Byte() & 7U) != 0U;
    break;
  default:
    s.hold_ratio = static_cast<double>(in.Byte()) / 255.0;
    s.noise_mv = static_cast<double>(in.Byte() % 8U);
    break;
  }
}

/// What the driver must return for a reading, from the recorded conversions.
struct Expected {
  Error error;
  int64_t temperature_mc;
  int64_t output_mv;
  int64_t spread_mv;
};

Expected Oracle(const mcp9700::Config& c, const FuzzPort& port) noexcept {
  const std::size_t total = static_cast<std::size_t>(c.discard_conversions) + c.samples;
  Check(port.recorded_count == total);
  const auto r = mcp9700_ref::Evaluate(c, &port.recorded[c.discard_conversions], c.samples);
  return {r.error, r.temperature_mc, r.output_mv, r.spread_mv};
}

/// The traffic promises of a reading that did not hit a failure (F4).
void CheckReadingTraffic(const mcp9700::Config& c, const FuzzPort& port,
                         bool start = false) noexcept {
  const int total = static_cast<int>(c.discard_conversions) + static_cast<int>(c.samples);
  Check(port.conversions == total);
  const int gaps = (c.sample_interval_us == 0U) ? 0 : (total - 1);
  Check(port.waits == (gaps + (start ? 1 : 0))); // a start adds its one settle wait
  Check(port.waited_us == ((static_cast<uint64_t>(gaps) * c.sample_interval_us) +
                           (start ? c.power_on_settle_us : 0U)));
}

/// F6: nothing out of range is ever `Ok`.
void CheckMeasurementSane(const mcp9700::Config& c, const mcp9700::Measurement& m) noexcept {
  Check(m.spread_mv <= c.max_spread_mv);
  const auto row = mcp9700_ref::RowOf(c);
  const int64_t top = (c.range == mcp9700::TemperatureClass::High) ? row.max_high_mc : row.max_mc;
  const int64_t raw = static_cast<int64_t>(m.temperature_mc) - c.offset_cal_mc;
  Check((raw >= (row.min_mc - c.range_margin_mc)) && (raw <= (top + c.range_margin_mc)));
}

void CheckNoStartTraffic(const FuzzPort& port) noexcept {
  Check((port.conversions == 0) && (port.gpio_writes == 0) && (port.bring_ups == 0) &&
        (port.waits == 0));
}

using Res = mcp9700::Result<void>;

void CheckStart(Driver& d, FuzzPort& port, const mcp9700::Config& c, bool was_up,
                const Res& r) noexcept {
  Check(KnownError(r.error) && (r.ok() == d.IsInitialized()));
  if (was_up) {
    Check(r.ok());
    CheckNoStartTraffic(port);
    return;
  }
  const bool valid = mcp9700_ref::Valid(c);
  Check(valid == (r.error != Error::InvalidArgument));
  if (!valid) {
    CheckNoStartTraffic(port);
    Check(port.logs == 0);
    return;
  }
  // One warning for a Custom part, one more when the switch-off after a failed start-up fails.
  Check(port.warnings ==
        (((c.variant == mcp9700::Variant::Custom) ? 1 : 0) + (port.inactive_write_failed ? 1 : 0)));
  const bool bus = !port.bus_up_ok || port.active_write_failed || port.conversion_failed;
  Check((r.error == Error::Bus) == bus);
  if (!port.bus_up_ok) {
    Check((port.conversions == 0) && (port.gpio_writes == 0));
  }
  if (r.ok()) {
    CheckReadingTraffic(c, port, true);
    Check(port.waited_us >= c.power_on_settle_us);
    Check(port.active_writes == 1);
    Check(Oracle(c, port).error == Error::Ok);
    Check(d.IsInitialized());
  } else {
    Check(!d.IsInitialized());
    if (r.error == Error::DeviceIdMismatch) {
      Check(Oracle(c, port).error != Error::Ok);
    }
    if (port.active_writes == 1) { // the sensor was powered: it is switched off
                                   // again when the write works
      Check(port.inactive_write_failed || (port.last_level_known && !port.last_level_active));
    }
  }
  const uint32_t knob =
      (c.power_on_settle_us > c.sample_interval_us) ? c.power_on_settle_us : c.sample_interval_us;
  Check(port.longest_wait <= knob);
}

/// A call that reads: the F1 to F8 promises.
template <typename R>
void CheckRead(Driver& d, FuzzPort& port, const mcp9700::Config& c, bool was_up,
               const R& r) noexcept {
  Check(KnownError(r.error));
  if (!was_up) {
    Check(r.error == Error::NotInitialized);
    CheckNoStartTraffic(port);
    Check(!d.IsInitialized());
    return;
  }
  Check(port.active_writes == 0);
  Check(d.IsInitialized());
  if (port.conversion_failed) {
    Check(r.error == Error::Bus);
    Check(port.conversions == (static_cast<int>(port.recorded_count) + 1));
    return;
  }
  Check(r.error != Error::Bus);
  CheckReadingTraffic(c, port);
  const Expected e = Oracle(c, port);
  Check(r.error == e.error);
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t* data, std::size_t size) {
  Input in(data, size);
  FuzzPort port(in);
  const mcp9700::Config config = FuzzConfig(in);
  Driver driver(port, config);
  Check(driver.GetConfig().samples == config.samples);
  Check(driver.GetSpec().ok() == mcp9700_ref::Valid(config));
  for (int calls = 0; calls < 64 && !in.Empty(); ++calls) {
    const uint8_t op = in.Byte() % 9U;
    const bool was_up = driver.IsInitialized();
    port.BeginCall();
    switch (op) {
    case 0: {
      const auto r = driver.EnsureInitialized();
      CheckStart(driver, port, config, was_up, r);
      break;
    }
    case 1: {
      const auto r = driver.Deinitialize();
      Check(KnownError(r.error) && !driver.IsInitialized());
      Check(port.active_writes == 0);
      Check((r.error == Error::Bus) == port.inactive_write_failed);
      if (!was_up) {
        Check(r.ok());
        CheckNoStartTraffic(port);
      } else if (r.ok()) {
        Check(port.last_level_known && !port.last_level_active);
      }
      break;
    }
    case 2: {
      const auto r = driver.Sleep();
      Check(KnownError(r.error) && !(driver.IsInitialized() && r.ok()));
      Check(port.active_writes == 0);
      if (!was_up) {
        Check(r.error == Error::NotInitialized);
        CheckNoStartTraffic(port);
      } else {
        Check((r.error == Error::Bus) == port.inactive_write_failed);
        Check(!driver.IsInitialized());
      }
      break;
    }
    case 3: {
      const auto r = driver.ReadMeasurement();
      CheckRead(driver, port, config, was_up, r);
      if (r.ok()) {
        const Expected e = Oracle(config, port);
        Check(r.value.temperature_mc == (e.temperature_mc + config.offset_cal_mc));
        Check(std::cmp_equal(r.value.output_mv, e.output_mv));
        Check(std::cmp_equal(r.value.spread_mv, e.spread_mv));
        CheckMeasurementSane(config, r.value);
      } else {
        Check((r.value.temperature_mc == 0) && (r.value.output_mv == 0U) &&
              (r.value.spread_mv == 0U));
      }
      break;
    }
    case 4: {
      const auto r = driver.ReadTemperatureMilliC();
      CheckRead(driver, port, config, was_up, r);
      Check(r.ok() || (r.value == 0));
      if (r.ok()) {
        Check(r.value == (Oracle(config, port).temperature_mc + config.offset_cal_mc));
      }
      break;
    }
    case 5: {
      const auto r = driver.ReadOutputMv();
      CheckRead(driver, port, config, was_up, r);
      Check(r.ok() || (r.value == 0U));
      if (r.ok()) {
        Check(std::cmp_equal(r.value, Oracle(config, port).output_mv));
      }
      break;
    }
    case 6: {
      const auto r = driver.ReadRawPortMv();
      Check(KnownError(r.error));
      if (!was_up) {
        Check(r.error == Error::NotInitialized);
        CheckNoStartTraffic(port);
      } else {
        Check(port.conversions == 1);
        Check((r.error == Error::Bus) == port.conversion_failed);
        Check(r.ok() ? (r.value == port.last_value) : (r.value == 0U));
      }
      break;
    }
    default:
      FuzzModel(port, in);
      break;
    }
    if (port.conversions > 0 && !port.conversion_failed) {
      Check(driver.GetLastRawPortMv() == port.last_value);
    }
  }
  return 0;
}
