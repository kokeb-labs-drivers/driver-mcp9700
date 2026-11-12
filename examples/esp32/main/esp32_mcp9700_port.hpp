/**
 * @file esp32_mcp9700_port.hpp
 * @brief ESP-IDF ports for the MCP970x driver: adc_oneshot with curve-fitting calibration, esp_rom
 * delays and ESP_LOG for the log hook. `Esp32Mcp9700Port` is for a sensor wired to the supply;
 * `Esp32Mcp9700SwitchedPort` also drives a GPIO that powers the sensor.
 */
#ifndef MCP9700_ESP32_MCP9700_PORT_HPP
#define MCP9700_ESP32_MCP9700_PORT_HPP

#include <cstdint>

#include "driver/gpio.h"
#include "esp_adc/adc_cali.h"
#include "esp_adc/adc_cali_scheme.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_log.h"
#include "esp_rom_sys.h"

#include "mcp9700.hpp"

/// Pins of the ESP-IDF ports.
struct Esp32Mcp9700Pins {
  adc_unit_t unit = ADC_UNIT_1;
  /// GPIO that powers the sensor through 200 ohm (datasheet section 4.2). Used by
  /// `Esp32Mcp9700SwitchedPort` only, which needs a value of 0 or more.
  int enable = -1;
};

/// ADC and pin plumbing shared by the two ports; not a driver port itself.
class Esp32Mcp9700Core {
public:
  /// The attenuation of the channel: 12 dB reaches about 3.1 V on the supported ESP32 parts.
  static constexpr adc_atten_t kAtten = ADC_ATTEN_DB_12;
  /// The conversion width in bits; the application declares the same value in
  /// `Config::adc_resolution_bits`.
  static constexpr adc_bitwidth_t kBits = ADC_BITWIDTH_12;
  /// Largest calibrated millivolt value the port reports at `kAtten`, to declare in
  /// `Config::adc_full_scale_mv`. It is not the supply: the calibrated range of a 12 dB channel
  /// ends below 3.3 V. This value is the example's estimate and is **not verified on hardware**;
  /// set it to the maximum your chip's calibration reaches.
  static constexpr uint32_t kAdcFullScaleMv = 3100;

  static_assert(kAtten == ADC_ATTEN_DB_12, "the declared full scale is the one of a 12 dB channel");
  static_assert(kBits == ADC_BITWIDTH_12, "Config::adc_resolution_bits is declared as 12");

  explicit Esp32Mcp9700Core(const Esp32Mcp9700Pins& pins) noexcept : pins_(pins) {}

  /// Bring the ADC up (and park the enable pin LOW when `use_enable`).
  bool Init(bool use_enable) noexcept {
    if (adc_ != nullptr) {
      return true;
    }
    if (use_enable) {
      if (pins_.enable < 0) {
        return false; // a switched port needs a pin: a wiring error, not a no-op
      }
      // The sensor starts unpowered: the level is set before the output is enabled. The driver
      // powers it with GpioSet(Enable, ACTIVE) during its own start-up.
      gpio_config_t io{};
      io.pin_bit_mask = 1ULL << static_cast<unsigned>(pins_.enable);
      io.mode = GPIO_MODE_OUTPUT;
      if (gpio_set_level(static_cast<gpio_num_t>(pins_.enable), 0) != ESP_OK ||
          gpio_config(&io) != ESP_OK) {
        return false;
      }
    }
    adc_oneshot_unit_init_cfg_t unit{};
    unit.unit_id = pins_.unit;
    if (adc_oneshot_new_unit(&unit, &adc_) != ESP_OK) {
      adc_ = nullptr;
      return false;
    }
    adc_cali_curve_fitting_config_t cali{};
    cali.unit_id = pins_.unit;
    cali.atten = kAtten;
    cali.bitwidth = kBits;
    // No fallback: a count scaled by a guess is not a calibrated millivolt value, so start-up fails
    // instead.
    if (adc_cali_create_scheme_curve_fitting(&cali, &cali_) != ESP_OK) {
      if (adc_oneshot_del_unit(adc_) != ESP_OK) {
        // The unit stays allocated; the port is unusable either way.
      }
      adc_ = nullptr;
      return false;
    }
    return true;
  }

  bool Read(uint8_t channel, uint32_t& voltage_mv) noexcept {
    if (adc_ == nullptr) {
      return false;
    }
    const auto ch = static_cast<adc_channel_t>(channel);
    if (configured_ != static_cast<int>(channel)) {
      adc_oneshot_chan_cfg_t cfg{};
      cfg.atten = kAtten;
      cfg.bitwidth = kBits;
      if (adc_oneshot_config_channel(adc_, ch, &cfg) != ESP_OK) {
        return false;
      }
      configured_ = static_cast<int>(channel);
    }
    int raw = 0;
    int mv = 0;
    if (adc_oneshot_read(adc_, ch, &raw) != ESP_OK ||
        adc_cali_raw_to_voltage(cali_, raw, &mv) != ESP_OK || mv < 0) {
      return false;
    }
    voltage_mv = static_cast<uint32_t>(mv);
    return true;
  }

  /// Drive the enable pin: powered = high.
  bool SetPower(bool on) noexcept {
    if (pins_.enable < 0) {
      return false;
    }
    return gpio_set_level(static_cast<gpio_num_t>(pins_.enable), on ? 1U : 0U) == ESP_OK;
  }

private:
  Esp32Mcp9700Pins pins_;
  adc_oneshot_unit_handle_t adc_ = nullptr;
  adc_cali_handle_t cali_ = nullptr;
  int configured_ = -1;
};

namespace esp32_mcp9700_detail {
inline void LogLine(mcp9700::LogLevel level, const char* message) noexcept {
  const esp_log_level_t esp_level =
      level == mcp9700::LogLevel::Error ? ESP_LOG_ERROR : ESP_LOG_INFO;
  esp_log_write(esp_level, "mcp9700", "%s\n", message); // plain text, never a format
}
} // namespace esp32_mcp9700_detail

/// The port for a sensor wired to the supply (VDD on 3V3): no `GpioSet()`, so `Sleep()` reports
/// `Unsupported` and the driver never tries to switch the sensor.
class Esp32Mcp9700Port : public mcp9700::AdcInterface<Esp32Mcp9700Port> {
public:
  static constexpr uint32_t kAdcFullScaleMv = Esp32Mcp9700Core::kAdcFullScaleMv;

  explicit Esp32Mcp9700Port(const Esp32Mcp9700Pins& pins = {}) noexcept : core_(pins) {}

  bool EnsureInitialized() noexcept {
    return core_.Init(false);
  }
  bool ReadVoltageMv(uint8_t channel, uint32_t& voltage_mv) noexcept {
    return core_.Read(channel, voltage_mv);
  }
  void DelayUs(uint32_t us) noexcept {
    esp_rom_delay_us(us);
  }
  void Log(mcp9700::LogLevel level, const char* message) noexcept {
    esp32_mcp9700_detail::LogLine(level, message);
  }

private:
  Esp32Mcp9700Core core_;
};

/// The port for a sensor powered from a GPIO through 200 ohm (datasheet section 4.2): it adds
/// `GpioSet()`, so `Sleep()` cuts the power. `Esp32Mcp9700Pins::enable` must be 0 or more.
class Esp32Mcp9700SwitchedPort : public mcp9700::AdcInterface<Esp32Mcp9700SwitchedPort> {
public:
  static constexpr uint32_t kAdcFullScaleMv = Esp32Mcp9700Core::kAdcFullScaleMv;

  explicit Esp32Mcp9700SwitchedPort(const Esp32Mcp9700Pins& pins) noexcept : core_(pins) {}

  bool EnsureInitialized() noexcept {
    return core_.Init(true);
  }
  bool ReadVoltageMv(uint8_t channel, uint32_t& voltage_mv) noexcept {
    return core_.Read(channel, voltage_mv);
  }
  void DelayUs(uint32_t us) noexcept {
    esp_rom_delay_us(us);
  }
  bool GpioSet(mcp9700::CtrlPin pin, mcp9700::GpioSignal signal) noexcept {
    return (pin == mcp9700::CtrlPin::Enable) &&
           core_.SetPower(signal == mcp9700::GpioSignal::ACTIVE);
  }
  void Log(mcp9700::LogLevel level, const char* message) noexcept {
    esp32_mcp9700_detail::LogLine(level, message);
  }

private:
  Esp32Mcp9700Core core_;
};

#endif // MCP9700_ESP32_MCP9700_PORT_HPP
