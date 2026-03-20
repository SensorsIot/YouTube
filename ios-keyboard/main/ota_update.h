#pragma once

#include "esp_err.h"

#define OTA_DEFAULT_URL "http://192.168.4.1:8080/firmware/ios-keyboard/ios-keyboard.bin"

/* Start OTA with the given URL (runs in background task).
 * Pass NULL to use OTA_DEFAULT_URL. */
esp_err_t ota_update_start(const char *url);
