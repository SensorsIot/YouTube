#pragma once

#include "esp_err.h"
#include <stdint.h>
#include <stdbool.h>

/* Initialize TinyUSB HID keyboard device. */
esp_err_t hid_keyboard_init(void);

/* Send a single key press + release. */
esp_err_t hid_keyboard_send_key(uint8_t scancode, uint8_t modifier);

/* Type a string using the active layout manager. */
esp_err_t hid_keyboard_send_string(const char *text);

/* Release all keys (all-keys-up report). */
esp_err_t hid_keyboard_release_all(void);

/* Is the USB host mounted/connected? */
bool hid_keyboard_is_mounted(void);
