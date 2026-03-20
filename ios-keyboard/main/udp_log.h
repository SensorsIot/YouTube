#pragma once

#include "esp_err.h"

#define UDP_LOG_DEFAULT_HOST "192.168.4.1"
#define UDP_LOG_DEFAULT_PORT 5555

/* Initialize UDP logging. Reads target from NVS; falls back to defaults. */
esp_err_t udp_log_init(void);

/* Initialize with explicit host:port (bypasses NVS). */
esp_err_t udp_log_init_explicit(const char *host, uint16_t port);
