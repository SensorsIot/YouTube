#include "udp_log.h"
#include "nvs_store.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/message_buffer.h"
#include "lwip/sockets.h"
#include <string.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "udp_log";

#define MSG_BUF_SIZE  4096
#define MAX_LOG_LINE  256

static MessageBufferHandle_t s_msg_buf;
static struct sockaddr_in s_dest_addr;
static vprintf_like_t s_orig_vprintf;

static int udp_log_vprintf(const char *fmt, va_list args)
{
    int ret = s_orig_vprintf(fmt, args);

    if (s_msg_buf) {
        char buf[MAX_LOG_LINE];
        int len = vsnprintf(buf, sizeof(buf), fmt, args);
        if (len > 0) {
            if (len >= (int)sizeof(buf)) len = sizeof(buf) - 1;
            xMessageBufferSendFromISR(s_msg_buf, buf, len, NULL);
        }
    }
    return ret;
}

static void udp_sender_task(void *arg)
{
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (sock < 0) {
        s_orig_vprintf("udp_log: failed to create socket\n", (va_list){0});
        vTaskDelete(NULL);
        return;
    }

    char buf[MAX_LOG_LINE];
    while (1) {
        size_t len = xMessageBufferReceive(s_msg_buf, buf, sizeof(buf), portMAX_DELAY);
        if (len > 0) {
            sendto(sock, buf, len, 0,
                   (struct sockaddr *)&s_dest_addr, sizeof(s_dest_addr));
        }
    }
}

esp_err_t udp_log_init_explicit(const char *host, uint16_t port)
{
    s_msg_buf = xMessageBufferCreate(MSG_BUF_SIZE);
    if (!s_msg_buf) return ESP_ERR_NO_MEM;

    memset(&s_dest_addr, 0, sizeof(s_dest_addr));
    s_dest_addr.sin_family = AF_INET;
    s_dest_addr.sin_port = htons(port);
    inet_aton(host, &s_dest_addr.sin_addr);

    xTaskCreate(udp_sender_task, "udp_log", 3072, NULL, 1, NULL);

    s_orig_vprintf = esp_log_set_vprintf(udp_log_vprintf);
    ESP_LOGI(TAG, "UDP logging -> %s:%d", host, port);
    return ESP_OK;
}

esp_err_t udp_log_init(void)
{
    char target[64] = {0};
    const char *host = UDP_LOG_DEFAULT_HOST;
    uint16_t port = UDP_LOG_DEFAULT_PORT;

    if (nvs_store_get_udp_target(target, sizeof(target))) {
        /* Parse "host:port" */
        char *colon = strchr(target, ':');
        if (colon) {
            *colon = '\0';
            host = target;
            port = (uint16_t)atoi(colon + 1);
        } else {
            host = target;
        }
        ESP_LOGI(TAG, "UDP target from NVS: %s:%d", host, port);
    } else {
        ESP_LOGI(TAG, "UDP target from defaults: %s:%d", host, port);
    }

    return udp_log_init_explicit(host, port);
}
