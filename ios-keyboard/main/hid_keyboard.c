#include "hid_keyboard.h"
#include "layout_manager.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>

/* esp_tinyusb managed component */
#include "tinyusb.h"
#include "class/hid/hid_device.h"

static const char *TAG = "hid_kbd";

static bool s_mounted = false;

/* ── TinyUSB descriptors ───────────────────────────────────────── */

#define TUSB_DESC_TOTAL_LEN (TUD_CONFIG_DESC_LEN + CFG_TUD_HID * TUD_HID_DESC_LEN)

/* Keyboard-only HID report descriptor */
const uint8_t hid_report_descriptor[] = {
    TUD_HID_REPORT_DESC_KEYBOARD(HID_REPORT_ID(HID_ITF_PROTOCOL_KEYBOARD)),
};

const char *hid_string_descriptor[] = {
    (char[]){0x09, 0x04},   /* 0: English */
    "iOS-Keyboard",          /* 1: Manufacturer */
    "iOS-KB HID Keyboard",   /* 2: Product */
    "000001",                /* 3: Serial */
    "HID Keyboard",          /* 4: HID interface */
};

static const uint8_t hid_configuration_descriptor[] = {
    TUD_CONFIG_DESCRIPTOR(1, 1, 0, TUSB_DESC_TOTAL_LEN,
                          TUSB_DESC_CONFIG_ATT_REMOTE_WAKEUP, 100),
    TUD_HID_DESCRIPTOR(0, 4, false, sizeof(hid_report_descriptor),
                       0x81, 16, 10),
};

/* ── TinyUSB HID callbacks ─────────────────────────────────────── */

uint8_t const *tud_hid_descriptor_report_cb(uint8_t instance)
{
    return hid_report_descriptor;
}

uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id,
                                hid_report_type_t report_type,
                                uint8_t *buffer, uint16_t reqlen)
{
    (void)instance; (void)report_id; (void)report_type;
    (void)buffer; (void)reqlen;
    return 0;
}

void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id,
                            hid_report_type_t report_type,
                            uint8_t const *buffer, uint16_t bufsize)
{
    /* LED status from host — ignore */
}

/* ── TinyUSB device callbacks ──────────────────────────────────── */

void tud_mount_cb(void)
{
    s_mounted = true;
    ESP_LOGI(TAG, "USB mounted");
}

void tud_umount_cb(void)
{
    s_mounted = false;
    ESP_LOGW(TAG, "USB unmounted");
}

void tud_suspend_cb(bool remote_wakeup_en)
{
    ESP_LOGI(TAG, "USB suspended");
}

void tud_resume_cb(void)
{
    ESP_LOGI(TAG, "USB resumed");
}

/* ── Internal helpers ──────────────────────────────────────────── */

static esp_err_t wait_hid_ready(void)
{
    for (int i = 0; i < 50 && !tud_hid_ready(); i++) {
        vTaskDelay(pdMS_TO_TICKS(2));
    }
    return tud_hid_ready() ? ESP_OK : ESP_ERR_TIMEOUT;
}

/* ── Public API ────────────────────────────────────────────────── */

esp_err_t hid_keyboard_init(void)
{
    const tinyusb_config_t tusb_cfg = {
        .device_descriptor = NULL,
        .string_descriptor = hid_string_descriptor,
        .string_descriptor_count = sizeof(hid_string_descriptor) / sizeof(hid_string_descriptor[0]),
        .external_phy = false,
#if (TUD_OPT_HIGH_SPEED)
        .fs_configuration_descriptor = hid_configuration_descriptor,
        .hs_configuration_descriptor = hid_configuration_descriptor,
        .qualifier_descriptor = NULL,
#else
        .configuration_descriptor = hid_configuration_descriptor,
#endif
    };

    esp_err_t err = tinyusb_driver_install(&tusb_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "TinyUSB install failed: %s", esp_err_to_name(err));
        return err;
    }

    ESP_LOGI(TAG, "HID keyboard initialized (TinyUSB)");
    return ESP_OK;
}

esp_err_t hid_keyboard_send_key(uint8_t scancode, uint8_t modifier)
{
    if (!s_mounted) return ESP_ERR_INVALID_STATE;

    esp_err_t err = wait_hid_ready();
    if (err != ESP_OK) return err;

    uint8_t keycode[6] = { scancode, 0, 0, 0, 0, 0 };
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, modifier, keycode);

    /* Hold key for 20ms (> 10ms poll interval) then release */
    vTaskDelay(pdMS_TO_TICKS(20));

    /* Release all keys */
    err = wait_hid_ready();
    if (err != ESP_OK) {
        /* Force release even on timeout */
        tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
        return err;
    }
    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);

    /* Wait for release report to be sent */
    vTaskDelay(pdMS_TO_TICKS(10));

    return ESP_OK;
}

esp_err_t hid_keyboard_send_string(const char *text)
{
    if (!text) return ESP_ERR_INVALID_ARG;

    for (const char *p = text; *p; p++) {
        uint8_t scancode = 0, modifier = 0;
        if (layout_manager_char_to_scancode(*p, &scancode, &modifier) == ESP_OK) {
            esp_err_t err = hid_keyboard_send_key(scancode, modifier);
            if (err != ESP_OK) return err;
        } else {
            ESP_LOGW(TAG, "No scancode for char 0x%02x '%c'", *p, *p);
        }
    }
    return ESP_OK;
}

esp_err_t hid_keyboard_release_all(void)
{
    if (!s_mounted) return ESP_ERR_INVALID_STATE;

    esp_err_t err = wait_hid_ready();
    if (err != ESP_OK) return err;

    tud_hid_keyboard_report(HID_ITF_PROTOCOL_KEYBOARD, 0, NULL);
    return ESP_OK;
}

bool hid_keyboard_is_mounted(void)
{
    return s_mounted;
}
