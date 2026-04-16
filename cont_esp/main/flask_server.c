#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_event.h"
#include "esp_http_client.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"
#include "esp_log.h"
#include "esp_lvgl_port.h"
#include "esp_netif.h"
#include "esp_timer.h"
#include "esp_wifi.h"
#include "lvgl.h"
#include "nvs_flash.h"
#include "pin_config.h"
#include "cJSON.h"
#include <freertos/FreeRTOS.h>
#include <freertos/event_groups.h>
#include <freertos/task.h>
#include <stdio.h>
#include <string.h>

#define SERVER_URL "http://3.19.32.161:5000/sensor"

static void send_sensor_data(char start_time[], char end_time[], char pace[]) {
  cJSON *root = cJSON_CreateObject();
  cJSON_AddNumberToObject(root, "start_time", start_time);
  cJSON_AddNumberToObject(root, "end_time", end_time);
  cJSON_AddNumberToObject(root, "pace", pace);

  char *json_data = cJSON_PrintUnformatted(root);

  esp_http_client_config_t config = {
      .url = SERVER_URL,
      .method = HTTP_METHOD_POST,
  };

  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_http_client_set_header(client, "Content-Type", "application/json");
  esp_http_client_set_post_field(client, json_data, strlen(json_data));

  esp_err_t err = esp_http_client_perform(client);
  if (err == ESP_OK) {
    ESP_LOGI(TAG, "POST status = %d", esp_http_client_get_status_code(client));
  } else {
    ESP_LOGE(TAG, "POST failed: %s", esp_err_to_name(err));
  }

  esp_http_client_cleanup(client);
  cJSON_free(json_data);
  cJSON_Delete(root);
}

static void wifi_event_handler(void *arg, esp_event_base_t event_base,
                               int32_t event_id, void *event_data) {
  if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
    esp_wifi_connect();
  } else if (event_base == WIFI_EVENT &&
             event_id == WIFI_EVENT_STA_DISCONNECTED) {
    esp_wifi_connect();
  } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
    xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
  }
}

static void wifi_init_sta(void) {
  s_wifi_event_group = xEventGroupCreate();

  ESP_ERROR_CHECK(nvs_flash_init());
  ESP_ERROR_CHECK(esp_netif_init());
  ESP_ERROR_CHECK(esp_event_loop_create_default());
  esp_netif_create_default_wifi_sta();

  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                             &wifi_event_handler, NULL));
  ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                             &wifi_event_handler, NULL));

  wifi_config_t wifi_config = {0};
  strcpy((char *)wifi_config.sta.ssid, "DukeVisitor");
  wifi_config.sta.threshold.authmode = WIFI_AUTH_OPEN;

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
  ESP_ERROR_CHECK(esp_wifi_start());

  xEventGroupWaitBits(s_wifi_event_group, WIFI_CONNECTED_BIT, pdFALSE, pdTRUE,
                      portMAX_DELAY);
}

static void one_second_timer_isr(void *arg) { start_bool = true; }

void app_main(void) {
  wifi_init_sta();

  esp_timer_create_args_t one_second_timer_args = {
      .callback = &one_second_timer_isr,
      .name = "one_second_timer",
  };
  esp_timer_handle_t one_second_timer;
  esp_timer_create(&one_second_timer_args, &one_second_timer);
  esp_timer_start_periodic(one_second_timer, 1000000);

  char new_text[512];

  while (1) {
    if (start_bool) {
      send_sensor_data("hehe", "hehe", "hehe");
      start_bool = false;
    }

    vTaskDelay(pdMS_TO_TICKS(10));
  }
}
