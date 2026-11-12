/**
 * @file mcp9700_basic_example.cpp
 * @brief Initialise the sensor from the menuconfig values and log the temperature twice a second.
 */
#include <cstdint>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp32_mcp9700_port.hpp"
#include "mcp9700.hpp"
#include "mcp9700_kconfig.hpp"

namespace {
constexpr const char* kTag = "mcp9700_example";

/// Prints milli-degrees as "23.456" with integer arithmetic only.
void LogTemperature(const mcp9700::Measurement& m) {
  const int32_t mc = m.temperature_mc;
  const uint32_t abs_mc = mc < 0 ? 0U - static_cast<uint32_t>(mc) : static_cast<uint32_t>(mc);
  ESP_LOGI(kTag, "%s%lu.%03lu C (VOUT %lu mV, spread %lu mV)", mc < 0 ? "-" : "",
           static_cast<unsigned long>(abs_mc / 1000U), static_cast<unsigned long>(abs_mc % 1000U),
           static_cast<unsigned long>(m.output_mv), static_cast<unsigned long>(m.spread_mv));
}
} // namespace

extern "C" void app_main(void) {
  // VDD on the supply (the wiring in the README): the port has no GpioSet(), so nothing is
  // switched. For a sensor powered from a pin, use Esp32Mcp9700SwitchedPort with
  // Esp32Mcp9700Pins{.enable = <gpio>} instead (README, "Switched supply").
  static Esp32Mcp9700Port port;
  // The menuconfig values, then the two facts that belong to the port: its full scale and its
  // resolution.
  mcp9700::Config config = mcp9700::KconfigDefaults();
  config.adc_full_scale_mv = Esp32Mcp9700Port::kAdcFullScaleMv;
  config.adc_resolution_bits = 12;
  static mcp9700::MCP9700<Esp32Mcp9700Port> sensor(port, config); // no I/O

  ESP_LOGI(kTag, "MCP970x driver %s", mcp9700::GetDriverVersion());
  if (const auto init = sensor.EnsureInitialized(); !init) {
    ESP_LOGE(kTag, "EnsureInitialized failed: %s", mcp9700::ToString(init.error));
    return;
  }
  while (true) {
    const auto reading = sensor.ReadMeasurement();
    if (reading) {
      LogTemperature(reading.value);
    } else {
      ESP_LOGW(kTag, "read failed: %s", mcp9700::ToString(reading.error));
    }
    vTaskDelay(pdMS_TO_TICKS(500));
  }
}
