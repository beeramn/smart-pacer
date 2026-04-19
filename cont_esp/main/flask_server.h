#ifndef SENSOR_HTTP_H
#define SENSOR_HTTP_H

#include "esp_err.h"
#include "freertos/event_groups.h"

#ifdef __cplusplus
extern "C" {
#endif

// =========================
// Global Definitions
// =========================

// WiFi event group + bit (used for connection sync)
extern EventGroupHandle_t s_wifi_event_group;
#define WIFI_CONNECTED_BIT BIT0

// =========================
// Function Declarations
// =========================

// Send sensor data to server (HTTP POST). Times are seconds since boot (same as esp_timer_get_time()/1e6).
void send_sensor_data(double start_time, double end_time, int minutes, int seconds);

// WiFi event handler
void wifi_event_handler(void *arg, esp_event_base_t event_base,
                        int32_t event_id, void *event_data);

// Initialize WiFi in station mode
void wifi_init_sta(void);

#ifdef __cplusplus
}
#endif

#endif // SENSOR_HTTP_H