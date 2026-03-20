#include "nvs_store.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "nvs_store";
static const char *NVS_NAMESPACE = "ios_kb";

esp_err_t nvs_store_init(void)
{
    esp_err_t err = nvs_flash_init();
    if (err == ESP_ERR_NVS_NO_FREE_PAGES || err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS corrupt, erasing...");
        ESP_ERROR_CHECK(nvs_flash_erase());
        err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(err);
    ESP_LOGI(TAG, "NVS initialized");
    return ESP_OK;
}

/* ── Generic helpers ───────────────────────────────────────────── */

static esp_err_t store_str(const char *key, const char *value)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_set_str(h, key, value);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

static bool load_str(const char *key, char *buf, size_t buf_len)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;

    esp_err_t err = nvs_get_str(h, key, buf, &buf_len);
    nvs_close(h);
    return (err == ESP_OK);
}

/* ── WiFi ──────────────────────────────────────────────────────── */

esp_err_t nvs_store_set_wifi(const char *ssid, const char *password)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_set_str(h, "wifi_ssid", ssid);
    if (err == ESP_OK) {
        err = nvs_set_str(h, "wifi_pass", password);
    }
    if (err == ESP_OK) {
        err = nvs_commit(h);
    }
    nvs_close(h);
    ESP_LOGI(TAG, "WiFi credentials saved (SSID: %s)", ssid);
    return err;
}

bool nvs_store_get_wifi(char *ssid, size_t ssid_len, char *password, size_t pass_len)
{
    nvs_handle_t h;
    if (nvs_open(NVS_NAMESPACE, NVS_READONLY, &h) != ESP_OK) return false;

    esp_err_t err = nvs_get_str(h, "wifi_ssid", ssid, &ssid_len);
    if (err == ESP_OK) {
        err = nvs_get_str(h, "wifi_pass", password, &pass_len);
    }
    nvs_close(h);
    return (err == ESP_OK);
}

esp_err_t nvs_store_erase_wifi(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    nvs_erase_key(h, "wifi_ssid");
    nvs_erase_key(h, "wifi_pass");
    err = nvs_commit(h);
    nvs_close(h);
    ESP_LOGI(TAG, "WiFi credentials erased");
    return err;
}

/* ── Keyboard layout ───────────────────────────────────────────── */

esp_err_t nvs_store_set_layout(const char *layout)
{
    ESP_LOGI(TAG, "Layout saved: %s", layout);
    return store_str("kb_layout", layout);
}

bool nvs_store_get_layout(char *layout, size_t len)
{
    return load_str("kb_layout", layout, len);
}

/* ── UDP log target ────────────────────────────────────────────── */

esp_err_t nvs_store_set_udp_target(const char *target)
{
    ESP_LOGI(TAG, "UDP target saved: %s", target);
    return store_str("udp_target", target);
}

bool nvs_store_get_udp_target(char *target, size_t len)
{
    return load_str("udp_target", target, len);
}

/* ── Device name ───────────────────────────────────────────────── */

esp_err_t nvs_store_set_device_name(const char *name)
{
    ESP_LOGI(TAG, "Device name saved: %s", name);
    return store_str("dev_name", name);
}

bool nvs_store_get_device_name(char *name, size_t len)
{
    return load_str("dev_name", name, len);
}

/* ── Factory reset ─────────────────────────────────────────────── */

esp_err_t nvs_store_erase_all(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_erase_all(h);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    ESP_LOGW(TAG, "All NVS data erased (factory reset)");
    return err;
}
