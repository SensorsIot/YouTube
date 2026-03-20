#include "esp_log.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs_store.h"
#include "udp_log.h"
#include "wifi_prov.h"
#include "ble_nus.h"
#include "http_server.h"
#include "watchdog.h"
#include "hid_keyboard.h"
#include "text_engine.h"
#include "layout_manager.h"

static const char *TAG = "app_main";

#define FW_VERSION "0.1.0"

/* Provide wifi_prov_is_connected to BLE module (weak override) */
bool wifi_prov_is_connected_extern(void)
{
    return wifi_prov_is_connected();
}

static void heartbeat_task(void *arg)
{
    uint32_t tick = 0;
    while (1) {
        int64_t uptime_us = esp_timer_get_time();
        uint32_t uptime_s = (uint32_t)(uptime_us / 1000000);
        ESP_LOGI(TAG, "heartbeat %"PRIu32" | wifi=%d ble=%d heap=%"PRIu32" uptime=%"PRIu32"s",
                 tick++, wifi_prov_is_connected(), ble_nus_is_connected(),
                 (uint32_t)esp_get_free_heap_size(), uptime_s);
        vTaskDelay(pdMS_TO_TICKS(10000));
    }
}

void app_main(void)
{
    ESP_LOGI(TAG, "=== iOS Keyboard Firmware v%s ===", FW_VERSION);

    /* 1. NVS — load config */
    nvs_store_init();

    /* 2. Network stack */
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    /* 3. UDP logging — reads target from NVS, captures all subsequent logs */
    udp_log_init();

    /* 4. WiFi — STA (stored creds) or AP (captive portal) */
    wifi_prov_init();

    /* 5. Wait for WiFi before BLE (coexistence) */
    if (!wifi_prov_is_ap_mode()) {
        ESP_LOGI(TAG, "Waiting for WiFi STA connection before starting BLE...");
        for (int i = 0; i < 150 && !wifi_prov_is_connected(); i++)
            vTaskDelay(pdMS_TO_TICKS(100));   /* up to 15s */
    }

    /* 6. BLE NUS — with device name from NVS */
    ble_nus_init();

    /* 7. HTTP server — /status, /ota, /layout, /reset */
    http_server_start();

    /* 8. Watchdog — task WDT + heap monitor */
    watchdog_init();

    /* 9. Phase 2: USB HID + layout + text engine */
    char layout[8] = "US";
    nvs_store_get_layout(layout, sizeof(layout));
    layout_manager_init(layout);
    text_engine_init();
    hid_keyboard_init();

    /* 10. Heartbeat — periodic status log */
    xTaskCreate(heartbeat_task, "heartbeat", 4096, NULL, 1, NULL);

    ESP_LOGI(TAG, "Init complete, running event-driven");
}
