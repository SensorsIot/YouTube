#include "ota_update.h"
#include "ble_nus.h"
#include "esp_log.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_https_ota.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "ota_update";

#define OTA_URL_MAX_LEN 256

typedef struct {
    char url[OTA_URL_MAX_LEN];
} ota_task_params_t;

static esp_err_t ota_http_event(esp_http_client_event_t *evt)
{
    switch (evt->event_id) {
    case HTTP_EVENT_ON_CONNECTED:
        ESP_LOGI(TAG, "HTTP connected");
        break;
    case HTTP_EVENT_ERROR:
        ESP_LOGE(TAG, "HTTP error");
        break;
    default:
        break;
    }
    return ESP_OK;
}

static void ota_task(void *arg)
{
    ota_task_params_t *params = (ota_task_params_t *)arg;
    ESP_LOGI(TAG, "Starting OTA from %s", params->url);

    esp_http_client_config_t http_cfg = {
        .url = params->url,
        .event_handler = ota_http_event,
        .keep_alive_enable = true,
    };

    esp_https_ota_config_t ota_cfg = {
        .http_config = &http_cfg,
    };

    esp_https_ota_handle_t ota_handle = NULL;
    esp_err_t ret = esp_https_ota_begin(&ota_cfg, &ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "OTA begin failed: %s", esp_err_to_name(ret));
        char msg[64];
        snprintf(msg, sizeof(msg), "ERR:O:%s", esp_err_to_name(ret));
        ble_nus_send(msg, strlen(msg));
        free(params);
        vTaskDelete(NULL);
        return;
    }

    int total = esp_https_ota_get_image_size(ota_handle);
    int last_pct = -1;

    while (1) {
        ret = esp_https_ota_perform(ota_handle);
        if (ret != ESP_ERR_HTTPS_OTA_IN_PROGRESS) break;

        int read = esp_https_ota_get_image_len_read(ota_handle);
        if (total > 0) {
            int pct = (read * 100) / total;
            if (pct / 10 != last_pct / 10) {
                last_pct = pct;
                ESP_LOGI(TAG, "OTA progress: %d%%", pct);
                char msg[32];
                snprintf(msg, sizeof(msg), "OTA:%d%%", pct);
                ble_nus_send(msg, strlen(msg));
            }
        }
    }

    if (ret == ESP_OK && esp_https_ota_is_complete_data_received(ota_handle)) {
        ret = esp_https_ota_finish(ota_handle);
        if (ret == ESP_OK) {
            ESP_LOGI(TAG, "OTA succeeded, rebooting...");
            ble_nus_send("OK:O:done", 9);
            vTaskDelay(pdMS_TO_TICKS(500));
            free(params);
            esp_restart();
        }
    }

    ESP_LOGE(TAG, "OTA failed: %s", esp_err_to_name(ret));
    esp_https_ota_abort(ota_handle);
    char msg[64];
    snprintf(msg, sizeof(msg), "ERR:O:%s", esp_err_to_name(ret));
    ble_nus_send(msg, strlen(msg));
    free(params);
    vTaskDelete(NULL);
}

esp_err_t ota_update_start(const char *url)
{
    ota_task_params_t *params = calloc(1, sizeof(ota_task_params_t));
    if (!params) return ESP_ERR_NO_MEM;

    strncpy(params->url, url ? url : OTA_DEFAULT_URL, OTA_URL_MAX_LEN - 1);

    BaseType_t ret = xTaskCreate(ota_task, "ota_task", 8192, params, 5, NULL);
    if (ret != pdPASS) {
        free(params);
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}
