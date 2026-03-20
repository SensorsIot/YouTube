#pragma once

#include "esp_err.h"
#include <stdbool.h>
#include <stddef.h>

esp_err_t nvs_store_init(void);

/* WiFi credentials */
esp_err_t nvs_store_set_wifi(const char *ssid, const char *password);
bool      nvs_store_get_wifi(char *ssid, size_t ssid_len, char *password, size_t pass_len);
esp_err_t nvs_store_erase_wifi(void);

/* Keyboard layout code (e.g. "US", "DE", "CH-DE") */
esp_err_t nvs_store_set_layout(const char *layout);
bool      nvs_store_get_layout(char *layout, size_t len);

/* UDP log target "host:port" (e.g. "192.168.4.1:5555") */
esp_err_t nvs_store_set_udp_target(const char *target);
bool      nvs_store_get_udp_target(char *target, size_t len);

/* BLE/AP device name */
esp_err_t nvs_store_set_device_name(const char *name);
bool      nvs_store_get_device_name(char *name, size_t len);

/* Factory reset — erase entire namespace */
esp_err_t nvs_store_erase_all(void);
