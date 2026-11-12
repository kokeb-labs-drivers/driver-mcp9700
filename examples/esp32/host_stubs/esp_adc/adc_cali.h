#ifndef HOST_STUB_ESP_ADC_ADC_CALI_H
#define HOST_STUB_ESP_ADC_ADC_CALI_H

#include "driver/gpio.h"

using adc_cali_handle_t = void*;

inline esp_err_t adc_cali_raw_to_voltage(adc_cali_handle_t /*handle*/, int raw, int* voltage_mv) {
  *voltage_mv = raw;
  return ESP_OK;
}

#endif // HOST_STUB_ESP_ADC_ADC_CALI_H
