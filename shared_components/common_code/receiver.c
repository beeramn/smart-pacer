#include <string.h>
#include <stdio.h>

#include "receiver.h"

#include "esp_log.h"
#include "esp_err.h"

#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include <stdbool.h>

static const char *TAG = "RECEIVER";

static uint8_t s_wifi_channel = 1;

// variables for saving pace
static volatile int s_latest_min = 0;
static volatile int s_latest_sec = 0;
static volatile bool s_have_new_pace = false;

typedef struct __attribute__((packed)) {
    int value1;
    int value2;
} espnow_int_msg_t;

// ---------------- RECEIVE CALLBACK ----------------
static void espnow_recv_cb(const esp_now_recv_info_t *recv_info,
                           const uint8_t *data, int len)
{
    
    if (len == sizeof(espnow_int_msg_t)) {
        espnow_int_msg_t msg;
        memcpy(&msg, data, sizeof(msg));
        
        // saving the values for car_esp.c
        s_latest_min = msg.value1;
        s_latest_sec = msg.value2;
        s_have_new_pace = true;

        printf("RECEIVED: %d, %d\n", msg.value1, msg.value2);

        const uint8_t *sender_mac = recv_info->src_addr;

        // Add sender as peer if not already present
        if (!esp_now_is_peer_exist(sender_mac)) {
            esp_now_peer_info_t peer = {0};
            memcpy(peer.peer_addr, sender_mac, ESP_NOW_ETH_ALEN);
            // peer.channel = 1;  // or match your actual channel
            peer.channel = s_wifi_channel;
            peer.ifidx = WIFI_IF_STA;
            peer.encrypt = false;

            esp_err_t add_err = esp_now_add_peer(&peer);
            if (add_err != ESP_OK && add_err != ESP_ERR_ESPNOW_EXIST) {
                ESP_LOGE(TAG, "Failed to add peer: %s", esp_err_to_name(add_err));
                return;
            }
        }

        const char *ack = "OK";

        esp_err_t err = esp_now_send(sender_mac,
                                     (const uint8_t *)ack,
                                     strlen(ack) + 1);

        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to send ACK: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI(TAG, "ACK sent");
        }
    } else {
        ESP_LOGW(TAG, "Received unknown data length: %d", len);
    }
}

// ---------------- INIT FUNCTION ----------------
void espnow_receiver_init(uint8_t wifi_channel)
{

    s_wifi_channel = wifi_channel;
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ESP_ERROR_CHECK(nvs_flash_init());
    } else {
        ESP_ERROR_CHECK(ret);
    }

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    ESP_ERROR_CHECK(esp_wifi_set_channel(wifi_channel, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    ESP_LOGI(TAG, "ESP-NOW receiver initialized on channel %d", wifi_channel);
}
// function for saving pace in car_esp
bool receiver_get_latest_pace(int *min_out, int *sec_out){
    if (!min_out || !sec_out) {
        return false;
    }

    if (!s_have_new_pace) {
        return false;
    }

    *min_out = s_latest_min;
    *sec_out = s_latest_sec;
    return true;
}