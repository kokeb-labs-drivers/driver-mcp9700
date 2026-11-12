#ifndef HOST_STUB_ESP_LOG_H
#define HOST_STUB_ESP_LOG_H

enum esp_log_level_t { ESP_LOG_ERROR, ESP_LOG_INFO };

inline void esp_log_write(esp_log_level_t /*level*/, const char* /*tag*/, const char* /*format*/,
                          ...) {}

#endif // HOST_STUB_ESP_LOG_H
