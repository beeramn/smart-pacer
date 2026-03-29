#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "get_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "transmit.h"
#include "ui.h"
#include "esp_log.h"
#include "nvs_flash.h"

static const char *TAG = "MAIN";
static ui_context_t g_ui;

static volatile bool s_stop_requested = false;
static volatile bool s_tx_running = false;

typedef struct {
    uint8_t mac[ESP_NOW_ETH_ALEN];
    int value1;
    int value2;
} tx_task_args_t;

static const uint8_t receiver_mac[ESP_NOW_ETH_ALEN] = {
    0x24, 0xEC, 0x4A, 0x52, 0xC3, 0x64
};

//backrground transmit trask 
static void tx_task(void *pvParameters)
{
    tx_task_args_t *args = (tx_task_args_t *)pvParameters;

    s_tx_running = true;

    transmit_two_ints(args->mac, args->value1, args->value2, &s_stop_requested);

    s_tx_running = false;
    free(args);
    vTaskDelete(NULL);
}

// function that happens when the start button is pressed


static void my_start_cb(lv_event_t *e)
{
    ui_context_t *ui = (ui_context_t *)lv_event_get_user_data(e);
    uint16_t min = ui_get_selected_minutes(ui);
    uint16_t sec = ui_get_selected_seconds(ui);

    ESP_LOGI(TAG, "Start pressed with pace %u:%02u", min, sec);

    if (s_tx_running) {
        ESP_LOGW(TAG, "Transmit task already running");
        return;
    }

    s_stop_requested = false;

    tx_task_args_t *args = malloc(sizeof(tx_task_args_t));
    if (args == NULL) {
        ESP_LOGE(TAG, "Failed to allocate tx task args");
        return;
    }

    memcpy(args->mac, receiver_mac, ESP_NOW_ETH_ALEN);
    args->value1 = min;
    args->value2 = sec;

    BaseType_t ok = xTaskCreate(
        tx_task,
        "tx_task",
        4096,
        args,
        5,
        NULL
    );

    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create tx task");
        free(args);
    }
}
//function that happens when the stop button is pressed
static void my_stop_cb(lv_event_t *e)
{
    (void)e;

    if (s_tx_running) {
        s_stop_requested = true;
        ESP_LOGI(TAG, "Stop requested");
    } else {
        ESP_LOGI(TAG, "No transmit task is running");
    }
}

void app_main(void)
{
    espnow_transmit_init(1);

    esp_log_level_set("*", ESP_LOG_INFO);

    // ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(ui_init(&g_ui));

    ui_set_start_callback(&g_ui, my_start_cb, &g_ui);
    ui_set_stop_callback(&g_ui, my_stop_cb, &g_ui);

    ESP_LOGI(TAG, "System ready");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}