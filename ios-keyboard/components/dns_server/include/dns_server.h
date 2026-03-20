/*
 * SPDX-FileCopyrightText: 2021-2023 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Unlicense OR CC0-1.0
 */

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifndef DNS_SERVER_MAX_ITEMS
#define DNS_SERVER_MAX_ITEMS 1
#endif

#define DNS_SERVER_CONFIG_SINGLE(queried_name, netif_key)  {        \
        .num_of_entries = 1,                                        \
        .item = { { .name = queried_name, .if_key = netif_key } }   \
        }

typedef struct dns_entry_pair {
    const char* name;
    const char* if_key;
    esp_ip4_addr_t ip;
} dns_entry_pair_t;

typedef struct dns_server_config {
    int num_of_entries;
    dns_entry_pair_t item[DNS_SERVER_MAX_ITEMS];
} dns_server_config_t;

typedef struct dns_server_handle *dns_server_handle_t;

dns_server_handle_t start_dns_server(dns_server_config_t *config);
void stop_dns_server(dns_server_handle_t handle);

#ifdef __cplusplus
}
#endif
