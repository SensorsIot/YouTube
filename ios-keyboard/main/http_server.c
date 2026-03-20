#include "http_server.h"
#include "wifi_prov.h"
#include "ble_nus.h"
#include "ota_update.h"
#include "nvs_store.h"
#include "hid_keyboard.h"
#include "esp_http_server.h"
#include "esp_ota_ops.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "cJSON.h"
#include <string.h>

#define HTTP_PORT_STA  80     /* STA mode: no portal conflict */
#define HTTP_PORT_AP   8080   /* AP mode: portal already on 80 */

static const char *TAG = "http_srv";

/* ── GET /status ───────────────────────────────────────────────── */

static esp_err_t status_handler(httpd_req_t *req)
{
    const esp_app_desc_t *app = esp_app_get_description();

    char layout[8] = "US";
    nvs_store_get_layout(layout, sizeof(layout));

    /* WiFi RSSI */
    int8_t rssi = 0;
    wifi_ap_record_t ap_info;
    if (wifi_prov_is_connected() && esp_wifi_sta_get_ap_info(&ap_info) == ESP_OK) {
        rssi = ap_info.rssi;
    }

    int64_t uptime_us = esp_timer_get_time();
    uint32_t uptime_s = (uint32_t)(uptime_us / 1000000);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "project", app->project_name);
    cJSON_AddStringToObject(root, "version", app->version);
    cJSON_AddStringToObject(root, "layout", layout);
    cJSON_AddBoolToObject(root, "wifi_connected", wifi_prov_is_connected());
    cJSON_AddNumberToObject(root, "wifi_rssi", rssi);
    cJSON_AddBoolToObject(root, "ble_connected", ble_nus_is_connected());
    cJSON_AddBoolToObject(root, "usb_mounted", hid_keyboard_is_mounted());
    cJSON_AddNumberToObject(root, "free_heap", (double)esp_get_free_heap_size());
    cJSON_AddNumberToObject(root, "uptime_s", uptime_s);

    const char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);

    cJSON_free((void *)json);
    cJSON_Delete(root);
    return ESP_OK;
}

/* ── POST /ota ─────────────────────────────────────────────────── */

static esp_err_t ota_handler(httpd_req_t *req)
{
    char buf[512];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);

    const char *url = NULL;

    if (len > 0) {
        buf[len] = '\0';
        cJSON *json = cJSON_Parse(buf);
        if (json) {
            url = cJSON_GetStringValue(cJSON_GetObjectItem(json, "url"));
            if (url) {
                ESP_LOGI(TAG, "OTA URL from request: %s", url);
            }
            /* Start OTA before deleting json — copy URL inside ota_update_start */
            esp_err_t err = ota_update_start(url);
            cJSON_Delete(json);

            httpd_resp_set_type(req, "application/json");
            if (err == ESP_OK) {
                httpd_resp_sendstr(req, "{\"status\":\"ok\",\"message\":\"OTA started\"}");
            } else {
                httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to start OTA");
            }
            return ESP_OK;
        }
    }

    /* No body or invalid JSON — use default URL */
    esp_err_t err = ota_update_start(NULL);
    httpd_resp_set_type(req, "application/json");
    if (err == ESP_OK) {
        httpd_resp_sendstr(req, "{\"status\":\"ok\",\"message\":\"OTA started (default URL)\"}");
    } else {
        httpd_resp_send_err(req, HTTPD_500_INTERNAL_SERVER_ERROR, "Failed to start OTA");
    }
    return ESP_OK;
}

/* ── GET /layout ───────────────────────────────────────────────── */

static esp_err_t layout_get_handler(httpd_req_t *req)
{
    char layout[8] = "US";
    nvs_store_get_layout(layout, sizeof(layout));

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "layout", layout);

    const char *json = cJSON_PrintUnformatted(root);
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, json);

    cJSON_free((void *)json);
    cJSON_Delete(root);
    return ESP_OK;
}

/* ── POST /layout ──────────────────────────────────────────────── */

static esp_err_t layout_post_handler(httpd_req_t *req)
{
    char buf[128];
    int len = httpd_req_recv(req, buf, sizeof(buf) - 1);
    if (len <= 0) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "No body");
        return ESP_FAIL;
    }
    buf[len] = '\0';

    cJSON *json = cJSON_Parse(buf);
    if (!json) {
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Invalid JSON");
        return ESP_FAIL;
    }

    const char *layout = cJSON_GetStringValue(cJSON_GetObjectItem(json, "layout"));
    if (!layout || strlen(layout) == 0) {
        cJSON_Delete(json);
        httpd_resp_send_err(req, HTTPD_400_BAD_REQUEST, "Missing layout");
        return ESP_FAIL;
    }

    nvs_store_set_layout(layout);
    ESP_LOGI(TAG, "Layout set to: %s", layout);
    cJSON_Delete(json);

    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\"}");
    return ESP_OK;
}

/* ── POST /reset ───────────────────────────────────────────────── */

static esp_err_t reset_handler(httpd_req_t *req)
{
    ESP_LOGW(TAG, "Factory reset requested via HTTP");
    httpd_resp_set_type(req, "application/json");
    httpd_resp_sendstr(req, "{\"status\":\"ok\",\"message\":\"Factory reset...\"}");

    vTaskDelay(pdMS_TO_TICKS(500));
    nvs_store_erase_all();
    esp_restart();
    return ESP_OK;
}

/* ── Server start ──────────────────────────────────────────────── */

esp_err_t http_server_start(void)
{
    httpd_handle_t server = NULL;
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();
    config.server_port = wifi_prov_is_ap_mode() ? HTTP_PORT_AP : HTTP_PORT_STA;
    config.ctrl_port = 32769;
    config.max_uri_handlers = 8;

    esp_err_t err = httpd_start(&server, &config);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to start HTTP server: %s", esp_err_to_name(err));
        return err;
    }

    static const httpd_uri_t uris[] = {
        { .uri = "/status",  .method = HTTP_GET,  .handler = status_handler },
        { .uri = "/ota",     .method = HTTP_POST, .handler = ota_handler },
        { .uri = "/layout",  .method = HTTP_GET,  .handler = layout_get_handler },
        { .uri = "/layout",  .method = HTTP_POST, .handler = layout_post_handler },
        { .uri = "/reset",   .method = HTTP_POST, .handler = reset_handler },
    };

    for (int i = 0; i < sizeof(uris) / sizeof(uris[0]); i++) {
        httpd_register_uri_handler(server, &uris[i]);
    }

    ESP_LOGI(TAG, "HTTP server started on port %d (/status, /ota, /layout, /reset)",
             config.server_port);
    return ESP_OK;
}
