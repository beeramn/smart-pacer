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
#include "flask_server.h"
#include "esp_timer.h"

volatile bool start_bool = false;

static const char *TAG = "MAIN";
static ui_context_t g_ui;

static volatile bool s_stop_requested = false;
static volatile bool s_tx_running = false;

/** Monotonic time from esp_timer_get_time(): when Start / Stop was last pressed (microseconds). */
static volatile int64_t s_pace_start_us;
static volatile int64_t s_pace_stop_us;

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
// helper function for launching task
static bool launch_tx_task(int value1, int value2){
    if (s_tx_running) {
        ESP_LOGW(TAG, "Transmit task already running");
        return false;
    }

    s_stop_requested = false;

    tx_task_args_t *args = malloc(sizeof(tx_task_args_t));
    if (args == NULL) {
        ESP_LOGE(TAG, "Failed to allocate tx task args");
        return false;
    }

    memcpy(args->mac, receiver_mac, ESP_NOW_ETH_ALEN);
    args->value1 = value1;
    args->value2 = value2;

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
        return false;
    }

    return true;
}

// function that happens when the start button is pressed
static void my_start_cb(lv_event_t *e)
{
    ui_context_t *ui = (ui_context_t *)lv_event_get_user_data(e);
    uint16_t min = ui_get_selected_minutes(ui);
    uint16_t sec = ui_get_selected_seconds(ui);

    s_pace_start_us = esp_timer_get_time();
    s_pace_stop_us = 0;

    ESP_LOGI(TAG, "Start pressed with pace %u:%02u (t_start=%lld us)", min, sec,
             (long long)s_pace_start_us);
    launch_tx_task((int)min, (int)sec);
}
// helper task to send stop packet when current tx ends
static void stop_send_task(void *pvParameters)
{
    (void)pvParameters;

    // wait for any active transmit task to stop
    while (s_tx_running) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    ESP_LOGI(TAG, "Sending stop pace 0:00");
    launch_tx_task(0, 0);

    vTaskDelete(NULL);
}
//function that happens when the stop button is pressed
static void my_stop_cb(lv_event_t *e){
    (void)e;
    s_pace_stop_us = esp_timer_get_time();
    ESP_LOGI(TAG, "Stop requested (t_stop=%lld us)", (long long)s_pace_stop_us);

    s_stop_requested = true;
    // launch a tiny helper task that waits, then sends 0:00
    BaseType_t ok = xTaskCreate(
        stop_send_task,
        "stop_send_task",
        3072,
        NULL,
        5,
        NULL
    );

    if (ok != pdPASS) {
        ESP_LOGE(TAG, "Failed to create stop_send_task");
    }
}

static void one_second_timer_isr(void *arg) { start_bool = true; }

void app_main(void)
{
    espnow_transmit_init(1);

    esp_log_level_set("*", ESP_LOG_INFO);

    // ESP_ERROR_CHECK(nvs_flash_init());
    ESP_ERROR_CHECK(ui_init(&g_ui));

    ui_set_start_callback(&g_ui, my_start_cb, &g_ui);
    ui_set_stop_callback(&g_ui, my_stop_cb, &g_ui);

    ESP_LOGI(TAG, "System ready");

    wifi_init_sta();
  
    while (1) {
        uint16_t min = ui_get_selected_minutes(&g_ui);
        uint16_t sec = ui_get_selected_seconds(&g_ui);

        double t_start_s =
            (s_pace_start_us > 0) ? (double)s_pace_start_us * 1e-6 : 0.0;
        double t_stop_s =
            (s_pace_stop_us > 0) ? (double)s_pace_stop_us * 1e-6 : 0.0;

        send_sensor_data(t_start_s, t_stop_s, (int)min, (int)sec);
        vTaskDelay(pdMS_TO_TICKS(100));
    }

}