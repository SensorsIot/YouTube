#pragma once

#include "esp_err.h"
#include <stdint.h>

/* HID modifier bits */
#define MOD_NONE    0x00
#define MOD_LCTRL   0x01
#define MOD_LSHIFT  0x02
#define MOD_LALT    0x04
#define MOD_LGUI    0x08
#define MOD_RCTRL   0x10
#define MOD_RSHIFT  0x20
#define MOD_RALT    0x40   /* AltGr on international layouts */
#define MOD_RGUI    0x80

/* Initialize layout manager with a layout code.
 * Supported: "US", "CH-DE", "DE", "FR", "UK", "ES", "IT" */
esp_err_t layout_manager_init(const char *layout_code);

/* Look up HID scancode + modifier for a character.
 * Returns ESP_OK on success, ESP_ERR_NOT_FOUND if unmapped. */
esp_err_t layout_manager_char_to_scancode(char c, uint8_t *scancode, uint8_t *modifier);

/* Get the current active layout code. */
const char *layout_manager_get_active(void);
