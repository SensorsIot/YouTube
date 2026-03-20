#include "ble_nus.h"

#if CONFIG_BT_ENABLED

#include "nvs_store.h"
#include "ota_update.h"
#include "text_engine.h"
#include "layout_manager.h"
#include "hid_keyboard.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_ota_ops.h"
#include "nvs_flash.h"
#include "nimble/nimble_port.h"
#include "nimble/nimble_port_freertos.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "cJSON.h"
#include <string.h>
#include <stdlib.h>

static const char *TAG = "ble_nus";

/* Forward declaration — strong definition in app_main.c */
extern bool wifi_prov_is_connected_extern(void);

/* NUS UUIDs (Nordic UART Service) */
static const ble_uuid128_t nus_svc_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x01, 0x00, 0x40, 0x6e);

static const ble_uuid128_t nus_rx_chr_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x02, 0x00, 0x40, 0x6e);

static const ble_uuid128_t nus_tx_chr_uuid =
    BLE_UUID128_INIT(0x9e, 0xca, 0xdc, 0x24, 0x0e, 0xe5, 0xa9, 0xe0,
                     0x93, 0xf3, 0xa3, 0xb5, 0x03, 0x00, 0x40, 0x6e);

static uint16_t s_tx_attr_handle;
static uint16_t s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint8_t s_own_addr_type;
static bool s_tx_subscribed = false;

static int nus_gap_event(struct ble_gap_event *event, void *arg);
static void nus_advertise(void);

/* ── TX send ───────────────────────────────────────────────────── */

esp_err_t ble_nus_send(const char *data, size_t len)
{
    if (s_conn_handle == BLE_HS_CONN_HANDLE_NONE || !s_tx_subscribed) {
        return ESP_ERR_INVALID_STATE;
    }

    struct os_mbuf *om = ble_hs_mbuf_from_flat(data, len);
    if (!om) return ESP_ERR_NO_MEM;

    int rc = ble_gatts_notify_custom(s_conn_handle, s_tx_attr_handle, om);
    return (rc == 0) ? ESP_OK : ESP_FAIL;
}

static void ble_send_str(const char *str)
{
    ble_nus_send(str, strlen(str));
}

/* ── Command dispatcher ────────────────────────────────────────── */

static void handle_command(const char *cmd, uint16_t len)
{
    if (len == 0) return;

    switch (cmd[0]) {
    case 'L': {
        /* L:<layout_code> — set keyboard layout + persist */
        if (len >= 3 && cmd[1] == ':') {
            const char *code = cmd + 2;
            layout_manager_init(code);
            nvs_store_set_layout(code);
            ESP_LOGI(TAG, "Layout set to: %s", code);
            char resp[32];
            snprintf(resp, sizeof(resp), "OK:L:%s", code);
            ble_send_str(resp);
        } else {
            ble_send_str("ERR:L:usage L:<code>");
        }
        break;
    }
    case 'S': {
        /* S — send status JSON */
        const esp_app_desc_t *app = esp_app_get_description();
        char layout[8] = "US";
        nvs_store_get_layout(layout, sizeof(layout));

        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "version", app->version);
        cJSON_AddStringToObject(root, "layout", layout);
        cJSON_AddBoolToObject(root, "wifi", wifi_prov_is_connected_extern());
        cJSON_AddNumberToObject(root, "heap", (double)esp_get_free_heap_size());

        char *json = cJSON_PrintUnformatted(root);
        if (json) {
            ble_send_str(json);
            cJSON_free(json);
        }
        cJSON_Delete(root);
        break;
    }
    case 'F': {
        /* F — factory reset */
        ESP_LOGW(TAG, "Factory reset via BLE");
        ble_send_str("OK:F:resetting");
        vTaskDelay(pdMS_TO_TICKS(500));
        nvs_store_erase_all();
        esp_restart();
        break;
    }
    case 'O': {
        /* O:<url> — trigger OTA */
        if (len >= 3 && cmd[1] == ':') {
            const char *url = cmd + 2;
            ESP_LOGI(TAG, "OTA via BLE: %s", url);
            ble_send_str("OK:O:starting");
            ota_update_start(url);
        } else {
            ble_send_str("ERR:O:usage O:<url>");
        }
        break;
    }
    case 'T': {
        /* T:<text> — type text */
        if (len >= 3 && cmd[1] == ':') {
            esp_err_t err = text_engine_type(cmd + 2);
            ble_send_str(err == ESP_OK ? "OK" : "ERR:T");
        } else {
            ble_send_str("ERR:T:usage T:<text>");
        }
        break;
    }
    case 'R': {
        /* R:<start>:<count>:<text> — replace range */
        if (len >= 5 && cmd[1] == ':') {
            int start = 0, count = 0;
            const char *p = cmd + 2;
            start = atoi(p);
            p = strchr(p, ':');
            if (p) {
                count = atoi(p + 1);
                p = strchr(p + 1, ':');
                if (p) {
                    esp_err_t err = text_engine_replace(start, count, p + 1);
                    ble_send_str(err == ESP_OK ? "OK" : "ERR:R");
                } else {
                    ble_send_str("ERR:R:format");
                }
            } else {
                ble_send_str("ERR:R:format");
            }
        } else {
            ble_send_str("ERR:R:usage R:<start>:<count>:<text>");
        }
        break;
    }
    case 'B': {
        /* B:<count> — backspace N characters */
        if (len >= 3 && cmd[1] == ':') {
            int count = atoi(cmd + 2);
            esp_err_t err = text_engine_backspace(count);
            ble_send_str(err == ESP_OK ? "OK" : "ERR:B");
        } else {
            ble_send_str("ERR:B:usage B:<count>");
        }
        break;
    }
    case 'C': {
        /* C — clear buffer */
        esp_err_t err = text_engine_clear();
        ble_send_str(err == ESP_OK ? "OK" : "ERR:C");
        break;
    }
    default:
        ESP_LOGW(TAG, "Unknown command: %c", cmd[0]);
        ble_send_str("ERR:unknown");
        break;
    }
}

/* ── GATT access callback ──────────────────────────────────────── */

static int nus_chr_access(uint16_t conn_handle, uint16_t attr_handle,
                          struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    if (ctxt->op == BLE_GATT_ACCESS_OP_WRITE_CHR) {
        uint16_t om_len = OS_MBUF_PKTLEN(ctxt->om);
        char buf[512];
        uint16_t copy_len = om_len < sizeof(buf) - 1 ? om_len : sizeof(buf) - 1;

        int rc = ble_hs_mbuf_to_flat(ctxt->om, buf, copy_len, NULL);
        if (rc == 0) {
            buf[copy_len] = '\0';
            ESP_LOGI(TAG, "RX %d bytes: %s", om_len, buf);
            handle_command(buf, copy_len);
        }
        return 0;
    }
    return BLE_ATT_ERR_UNLIKELY;
}

/* ── GATT service definition ───────────────────────────────────── */

static const struct ble_gatt_svc_def nus_gatt_svcs[] = {
    {
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = &nus_svc_uuid.u,
        .characteristics = (struct ble_gatt_chr_def[]) {
            {
                .uuid = &nus_rx_chr_uuid.u,
                .access_cb = nus_chr_access,
                .flags = BLE_GATT_CHR_F_WRITE | BLE_GATT_CHR_F_WRITE_NO_RSP,
            },
            {
                .uuid = &nus_tx_chr_uuid.u,
                .access_cb = nus_chr_access,
                .val_handle = &s_tx_attr_handle,
                .flags = BLE_GATT_CHR_F_NOTIFY,
            },
            { 0 },
        },
    },
    { 0 },
};

/* ── Advertising ───────────────────────────────────────────────── */

static void nus_advertise(void)
{
    struct ble_hs_adv_fields fields = {};
    fields.flags = BLE_HS_ADV_F_DISC_GEN | BLE_HS_ADV_F_BREDR_UNSUP;

    const char *name = ble_svc_gap_device_name();
    fields.name = (uint8_t *)name;
    fields.name_len = strlen(name);
    fields.name_is_complete = 1;

    int rc = ble_gap_adv_set_fields(&fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_set_fields failed: %d", rc);
        return;
    }

    struct ble_hs_adv_fields rsp_fields = {};
    rsp_fields.uuids128 = (ble_uuid128_t[]){ nus_svc_uuid };
    rsp_fields.num_uuids128 = 1;
    rsp_fields.uuids128_is_complete = 1;

    rc = ble_gap_adv_rsp_set_fields(&rsp_fields);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_rsp_set_fields failed: %d", rc);
        return;
    }

    struct ble_gap_adv_params adv_params = {};
    adv_params.conn_mode = BLE_GAP_CONN_MODE_UND;
    adv_params.disc_mode = BLE_GAP_DISC_MODE_GEN;

    rc = ble_gap_adv_start(s_own_addr_type, NULL, BLE_HS_FOREVER,
                           &adv_params, nus_gap_event, NULL);
    if (rc != 0) {
        ESP_LOGE(TAG, "adv_start failed: %d", rc);
    }
}

/* ── GAP event handler ─────────────────────────────────────────── */

static int nus_gap_event(struct ble_gap_event *event, void *arg)
{
    switch (event->type) {
    case BLE_GAP_EVENT_LINK_ESTAB:
        if (event->connect.status == 0) {
            s_conn_handle = event->connect.conn_handle;
            ESP_LOGI(TAG, "Connected, handle=%d", s_conn_handle);
        } else {
            ESP_LOGW(TAG, "Connection failed, status=%d", event->connect.status);
            nus_advertise();
        }
        break;

    case BLE_GAP_EVENT_DISCONNECT:
        ESP_LOGI(TAG, "Disconnected, reason=%d", event->disconnect.reason);
        s_conn_handle = BLE_HS_CONN_HANDLE_NONE;
        s_tx_subscribed = false;
        nus_advertise();
        break;

    case BLE_GAP_EVENT_ADV_COMPLETE:
        nus_advertise();
        break;

    case BLE_GAP_EVENT_MTU:
        ESP_LOGI(TAG, "MTU updated: %d", event->mtu.value);
        break;

    case BLE_GAP_EVENT_SUBSCRIBE:
        ESP_LOGI(TAG, "Subscribe: cur_notify=%d", event->subscribe.cur_notify);
        s_tx_subscribed = event->subscribe.cur_notify;
        break;

    case BLE_GAP_EVENT_REPEAT_PAIRING: {
        struct ble_gap_conn_desc desc;
        ble_gap_conn_find(event->repeat_pairing.conn_handle, &desc);
        ble_store_util_delete_peer(&desc.peer_id_addr);
        return BLE_GAP_REPEAT_PAIRING_RETRY;
    }
    }
    return 0;
}

/* ── Host sync callback ────────────────────────────────────────── */

static void nus_on_sync(void)
{
    int rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        ESP_LOGE(TAG, "ensure_addr failed: %d", rc);
        return;
    }

    rc = ble_hs_id_infer_auto(0, &s_own_addr_type);
    if (rc != 0) {
        ESP_LOGE(TAG, "infer_auto failed: %d", rc);
        return;
    }

    uint8_t addr[6];
    ble_hs_id_copy_addr(s_own_addr_type, addr, NULL);
    ESP_LOGI(TAG, "BLE addr: %02x:%02x:%02x:%02x:%02x:%02x",
             addr[5], addr[4], addr[3], addr[2], addr[1], addr[0]);

    nus_advertise();
}

static void nus_on_reset(int reason)
{
    ESP_LOGW(TAG, "BLE host reset, reason=%d", reason);
}

static void nus_host_task(void *param)
{
    ESP_LOGI(TAG, "BLE host task started");
    nimble_port_run();
    nimble_port_freertos_deinit();
}

/* ── Weak extern for wifi status (avoids circular dependency) ─── */

bool __attribute__((weak)) wifi_prov_is_connected_extern(void)
{
    return false;
}

/* ── Public API ────────────────────────────────────────────────── */

void ble_store_config_init(void);

esp_err_t ble_nus_init(void)
{
    esp_err_t ret = nimble_port_init();
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "nimble_port_init failed: %d", ret);
        return ret;
    }

    ble_hs_cfg.reset_cb = nus_on_reset;
    ble_hs_cfg.sync_cb = nus_on_sync;

    int rc = ble_gatts_count_cfg(nus_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "gatts_count_cfg failed: %d", rc);
        return ESP_FAIL;
    }

    rc = ble_gatts_add_svcs(nus_gatt_svcs);
    if (rc != 0) {
        ESP_LOGE(TAG, "gatts_add_svcs failed: %d", rc);
        return ESP_FAIL;
    }

    ble_svc_gap_init();
    ble_svc_gatt_init();

    /* Set BLE device name from NVS or default */
    char dev_name[32] = {0};
    if (!nvs_store_get_device_name(dev_name, sizeof(dev_name)) || strlen(dev_name) == 0) {
        uint8_t mac[6];
        esp_read_mac(mac, ESP_MAC_WIFI_STA);
        snprintf(dev_name, sizeof(dev_name), "iOS-KB-%02X%02X", mac[4], mac[5]);
    }

    rc = ble_svc_gap_device_name_set(dev_name);
    if (rc != 0) {
        ESP_LOGW(TAG, "gap_device_name_set failed: %d", rc);
    }

    ble_store_config_init();
    nimble_port_freertos_init(nus_host_task);

    ESP_LOGI(TAG, "BLE NUS initialized (device: %s)", dev_name);
    return ESP_OK;
}

bool ble_nus_is_connected(void)
{
    return s_conn_handle != BLE_HS_CONN_HANDLE_NONE;
}

#endif /* CONFIG_BT_ENABLED */
