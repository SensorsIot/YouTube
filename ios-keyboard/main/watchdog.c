#include "watchdog.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "watchdog";

#define HEAP_WARN_THRESHOLD   (50 * 1024)   /* 50 KB */
#define HEAP_REBOOT_THRESHOLD (30 * 1024)   /* 30 KB */
#define HEARTBEAT_INTERVAL_MS 30000         /* 30 seconds */

static void watchdog_task(void *arg)
{
    /* Subscribe this task to the task watchdog */
    esp_task_wdt_add(NULL);

    while (1) {
        /* Reset watchdog timer */
        esp_task_wdt_reset();

        /* Check heap */
        uint32_t free_heap = esp_get_free_heap_size();
        uint32_t min_heap = esp_get_minimum_free_heap_size();

        if (free_heap < HEAP_REBOOT_THRESHOLD) {
            ESP_LOGE(TAG, "CRITICAL: free heap %"PRIu32" < %d, rebooting!",
                     free_heap, HEAP_REBOOT_THRESHOLD);
            vTaskDelay(pdMS_TO_TICKS(100));
            esp_restart();
        } else if (free_heap < HEAP_WARN_THRESHOLD) {
            ESP_LOGW(TAG, "LOW HEAP: free=%"PRIu32" min=%"PRIu32, free_heap, min_heap);
        }

        vTaskDelay(pdMS_TO_TICKS(HEARTBEAT_INTERVAL_MS));
    }
}

esp_err_t watchdog_init(void)
{
    /* Initialize the task watchdog (if not already initialized by system) */
    esp_task_wdt_config_t wdt_cfg = {
        .timeout_ms = 60000,
        .idle_core_mask = 0,
        .trigger_panic = true,
    };
    esp_err_t err = esp_task_wdt_reconfigure(&wdt_cfg);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "task_wdt_reconfigure: %s (may already be configured)", esp_err_to_name(err));
    }

    BaseType_t ret = xTaskCreate(watchdog_task, "watchdog", 3072, NULL, 2, NULL);
    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create watchdog task");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Watchdog started (WDT=60s, heap warn=%dKB, reboot=%dKB)",
             HEAP_WARN_THRESHOLD / 1024, HEAP_REBOOT_THRESHOLD / 1024);
    return ESP_OK;
}
