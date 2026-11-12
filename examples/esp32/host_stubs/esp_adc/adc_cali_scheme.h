#ifndef HOST_STUB_ESP_ADC_ADC_CALI_SCHEME_H
#define HOST_STUB_ESP_ADC_ADC_CALI_SCHEME_H

#include "adc_cali.h"
#include "adc_oneshot.h"

struct adc_cali_curve_fitting_config_t {
  adc_unit_t unit_id;
  adc_atten_t atten;
  adc_bitwidth_t bitwidth;
};

inline esp_err_t adc_cali_create_scheme_curve_fitting(
    const adc_cali_curve_fitting_config_t* /*config*/, adc_cali_handle_t* handle) {
  *handle = nullptr;
  return ESP_OK;
}

#endif // HOST_STUB_ESP_ADC_ADC_CALI_SCHEME_H
