#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

#if CONFIG_BT_ENABLED
esp_err_t ble_nus_init(void);
bool      ble_nus_is_connected(void);
esp_err_t ble_nus_send(const char *data, size_t len);
#else
static inline esp_err_t ble_nus_init(void) { return ESP_OK; }
static inline bool ble_nus_is_connected(void) { return false; }
static inline esp_err_t ble_nus_send(const char *data, size_t len) { return ESP_ERR_NOT_SUPPORTED; }
#endif
