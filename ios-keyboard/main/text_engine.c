#include "text_engine.h"
#include "hid_keyboard.h"
#include "layout_manager.h"
#include "esp_log.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "text_eng";

#define KEY_BACKSPACE 0x2A

static char s_buffer[TEXT_ENGINE_MAX_BUF];
static int s_len = 0;

esp_err_t text_engine_init(void)
{
    memset(s_buffer, 0, sizeof(s_buffer));
    s_len = 0;
    ESP_LOGI(TAG, "Text engine initialized");
    return ESP_OK;
}

esp_err_t text_engine_type(const char *text)
{
    if (!text) return ESP_ERR_INVALID_ARG;

    int text_len = strlen(text);
    if (s_len + text_len >= TEXT_ENGINE_MAX_BUF) {
        ESP_LOGW(TAG, "Buffer overflow, truncating");
        text_len = TEXT_ENGINE_MAX_BUF - s_len - 1;
        if (text_len <= 0) return ESP_ERR_NO_MEM;
    }

    /* Type each character via HID */
    esp_err_t err = hid_keyboard_send_string(text);

    /* Update buffer */
    memcpy(s_buffer + s_len, text, text_len);
    s_len += text_len;
    s_buffer[s_len] = '\0';

    return err;
}

static esp_err_t emit_backspaces(int count)
{
    for (int i = 0; i < count; i++) {
        esp_err_t err = hid_keyboard_send_key(KEY_BACKSPACE, 0);
        if (err != ESP_OK) return err;
    }
    return ESP_OK;
}

esp_err_t text_engine_backspace(int count)
{
    if (count <= 0) return ESP_OK;
    if (count > s_len) count = s_len;

    esp_err_t err = emit_backspaces(count);

    s_len -= count;
    s_buffer[s_len] = '\0';

    return err;
}

esp_err_t text_engine_replace(int start, int count, const char *text)
{
    if (!text) return ESP_ERR_INVALID_ARG;
    if (start < 0 || start > s_len) return ESP_ERR_INVALID_ARG;
    if (count < 0) count = 0;
    if (start + count > s_len) count = s_len - start;

    int new_len = strlen(text);
    int end = start + count;

    /* Diff algorithm: find how many chars from end of buffer we need to erase
     * and retype. We need to backspace from current cursor (end of buffer)
     * to the replacement point, do the replacement, then retype the tail. */

    /* Characters after the replaced region */
    int tail_len = s_len - end;

    /* Total backspaces needed: tail + count (to get to start position) */
    int total_bs = tail_len + count;

    ESP_LOGI(TAG, "replace: start=%d count=%d new_len=%d tail=%d bs=%d",
             start, count, new_len, tail_len, total_bs);

    /* Emit backspaces */
    esp_err_t err = emit_backspaces(total_bs);
    if (err != ESP_OK) return err;

    /* Build new buffer content */
    char tail[TEXT_ENGINE_MAX_BUF];
    if (tail_len > 0) {
        memcpy(tail, s_buffer + end, tail_len);
    }

    /* Update buffer */
    int final_len = start + new_len + tail_len;
    if (final_len >= TEXT_ENGINE_MAX_BUF) {
        ESP_LOGW(TAG, "Buffer overflow in replace");
        return ESP_ERR_NO_MEM;
    }

    memcpy(s_buffer + start, text, new_len);
    if (tail_len > 0) {
        memcpy(s_buffer + start + new_len, tail, tail_len);
    }
    s_len = final_len;
    s_buffer[s_len] = '\0';

    /* Type the replacement text + tail */
    err = hid_keyboard_send_string(text);
    if (err != ESP_OK) return err;

    if (tail_len > 0) {
        tail[tail_len] = '\0';
        err = hid_keyboard_send_string(tail);
    }

    return err;
}

esp_err_t text_engine_clear(void)
{
    /* Backspace everything */
    esp_err_t err = ESP_OK;
    if (s_len > 0) {
        err = emit_backspaces(s_len);
    }

    s_len = 0;
    s_buffer[0] = '\0';

    hid_keyboard_release_all();

    ESP_LOGI(TAG, "Buffer cleared");
    return err;
}

int text_engine_get_length(void)
{
    return s_len;
}
