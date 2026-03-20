#pragma once

#include "esp_err.h"

/* Start task watchdog and heap monitor.
 * - Task WDT: heartbeat every 30s, 60s timeout
 * - Heap monitor: warn at 50KB, reboot at 30KB */
esp_err_t watchdog_init(void);
