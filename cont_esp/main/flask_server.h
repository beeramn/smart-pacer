#ifndef CONTROLLER_H
#define CONTROLLER_H

#include <stdbool.h>
#include "freertos/FreeRTOS.h"
#include "freertos/event_groups.h"
#include "esp_event.h"
#include "esp_err.h"
#include "esp_http_client.h"
#include "esp_timer.h"
#include "esp_wifi.h"

// Shared globals used by this source file
extern EventGroupHandle_t s_wifi_event_group;
extern const int WIFI_CONNECTED_BIT;
extern bool start_bool;
extern const char *TAG;

// Main entry point
void app_main(void);

#endif // CONTROLLER_H