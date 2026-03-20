#pragma once

#include "esp_err.h"

/* Maximum text buffer size (characters) */
#define TEXT_ENGINE_MAX_BUF 4096

/* Initialize the text engine with an empty buffer. */
esp_err_t text_engine_init(void);

/* Append text — emit HID keystrokes for each character. */
esp_err_t text_engine_type(const char *text);

/* Replace a range in the buffer using minimal backspace + retype.
 * start: character offset from start of buffer
 * count: number of characters to replace
 * text: replacement text */
esp_err_t text_engine_replace(int start, int count, const char *text);

/* Emit N backspaces and update buffer accordingly. */
esp_err_t text_engine_backspace(int count);

/* Clear the entire buffer and release all keys. */
esp_err_t text_engine_clear(void);

/* Get current buffer length (characters). */
int text_engine_get_length(void);
