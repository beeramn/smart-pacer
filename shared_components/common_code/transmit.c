#include <string.h>
#include <stdio.h>
#include <stdbool.h>

#include "transmit.h"

#include "esp_log.h"
#include "esp_err.h"

#include "nvs_flash.h"
#include "esp_netif.h"
#include "esp_event.h"
#include "esp_wifi.h"
#include "esp_now.h"
#include "esp_wifi_types.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"

static const char *TAG = "TRANSMIT";
static uint8_t s_wifi_channel = 1;
static volatile bool s_ack_received = false;

static uint8_t num_acks = 3;

static EventGroupHandle_t s_wifi_event_group;
#define WIFI_STARTED_BIT BIT0

static void wifi_event_handler(void *arg,
                               esp_event_base_t event_base,
                               int32_t event_id,
                               void *event_data)
{
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        xEventGroupSetBits(s_wifi_event_group, WIFI_STARTED_BIT);
        ESP_LOGI(TAG, "Wi-Fi STA started");
    }
}

static void espnow_send_cb(const wifi_tx_info_t *tx_info,
                           esp_now_send_status_t status)
{
    if (tx_info) {
        const uint8_t *da = tx_info->des_addr;
        printf("ESP-NOW send to %02X:%02X:%02X:%02X:%02X:%02X -> %s\n",
               da[0], da[1], da[2], da[3], da[4], da[5],
               status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
    } else {
        printf("ESP-NOW send -> %s\n",
               status == ESP_NOW_SEND_SUCCESS ? "OK" : "FAIL");
    }
}

static void espnow_recv_cb(const esp_now_recv_info_t *recv_info,
                           const uint8_t *data, int len)
{
    if (data == NULL || len <= 0) {
        ESP_LOGW(TAG, "Received empty ACK/data");
        return;
    }

    if (len >= 2 && memcmp(data, "OK", 2) == 0){
        s_ack_received = true;

        if (recv_info && recv_info->src_addr) {
            const uint8_t *sa = recv_info->src_addr;
            printf("ACK received from %02X:%02X:%02X:%02X:%02X:%02X: %.*s\n",
                   sa[0], sa[1], sa[2], sa[3], sa[4], sa[5], len, (const char *)data);
        } else {
            printf("ACK received: %.*s\n", len, (const char *)data);
        }
    } else {
        printf("Received non-ACK data: %.*s\n", len, (const char *)data);
    }
}

void espnow_transmit_init(uint8_t wifi_channel)
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

    s_wifi_event_group = xEventGroupCreate();
    if (s_wifi_event_group == NULL) {
        ESP_LOGE(TAG, "Failed to create Wi-Fi event group");
        return;
    }

    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT,
                                               ESP_EVENT_ANY_ID,
                                               &wifi_event_handler,
                                               NULL));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_start());

    // Wait here until Wi-Fi is fully started
    xEventGroupWaitBits(s_wifi_event_group,
                        WIFI_STARTED_BIT,
                        pdFALSE,
                        pdTRUE,
                        portMAX_DELAY);

    ESP_ERROR_CHECK(esp_wifi_set_channel(s_wifi_channel, WIFI_SECOND_CHAN_NONE));

    ESP_ERROR_CHECK(esp_now_init());
    ESP_ERROR_CHECK(esp_now_register_send_cb(espnow_send_cb));
    ESP_ERROR_CHECK(esp_now_register_recv_cb(espnow_recv_cb));

    ESP_LOGI(TAG, "ESP-NOW transmit initialized on channel %d", wifi_channel);
}

void transmit_two_ints(const uint8_t peer_mac[ESP_NOW_ETH_ALEN],
                       int value1,
                       int value2,
                       volatile bool *stop_requested)
{
    espnow_int_msg_t msg = {
        .value1 = value1,
        .value2 = value2
    };

    int ack_count = 0;

    esp_now_peer_info_t peer = {0};
    memcpy(peer.peer_addr, peer_mac, ESP_NOW_ETH_ALEN);
    peer.ifidx = WIFI_IF_STA;
    peer.channel = s_wifi_channel;
    peer.encrypt = false;

    esp_err_t err = esp_now_add_peer(&peer);
    if (err != ESP_OK && err != ESP_ERR_ESPNOW_EXIST) {
        ESP_LOGE(TAG, "Failed to add peer: %s", esp_err_to_name(err));
        return;
    }

    while (ack_count < num_acks) {
        if (stop_requested && *stop_requested) {
            ESP_LOGI(TAG, "Transmission stopped by user");
            break;
        }

        s_ack_received = false;

        err = esp_now_send(peer_mac, (uint8_t *)&msg, sizeof(msg));
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_now_send failed: %s", esp_err_to_name(err));
            continue;
        }

        ESP_LOGI(TAG, "Sent ints: %d, %d (ACKs: %d/%d)", value1, value2, ack_count, num_acks);

        int wait_ms = 0;
        while (!s_ack_received && wait_ms < 500) {
            if (stop_requested && *stop_requested) {
                ESP_LOGI(TAG, "Transmission stopped while waiting for ACK");
                return;
            }

            vTaskDelay(pdMS_TO_TICKS(50));
            wait_ms += 50;
        }

        if (s_ack_received) {
            ack_count++;
            ESP_LOGI(TAG, "ACK received (%d/%d)", ack_count, num_acks);
        } else {
            ESP_LOGW(TAG, "No ACK received, retrying...");
        }

        int delay_ms = 0;
        while (delay_ms < 5000) {
            if (stop_requested && *stop_requested) {
                ESP_LOGI(TAG, "Transmission stopped during retry delay");
                return;
            }

            vTaskDelay(pdMS_TO_TICKS(50));
            delay_ms += 50;
        }
    }

    ESP_LOGI(TAG, "Finished transmit loop");
    ESP_LOGI(TAG, "Finished: received 100 ACKs");
}