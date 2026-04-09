// #include "get_mac.h"
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "receiver.h"

// void app_main(void)
// {
//     // This function gets the mac address (needed for ESP-NOW) of the esp box 
//     // and prints to the monitor. 
//     get_mac();

//     //This function will listen for a pacemessage(2 ints) 
//     // will send OK to transmitter 
//     espnow_receiver_init(1);

// }

#include <stdio.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "receiver.h"
#include "speedcontroller.h"

static const char *TAG = "CAR";

// Encoder pulse input (one wire). Counts rising edges.
#define ENCODER_GPIO 10

// interupt for encouder sum count
static volatile int32_t s_encoder_count;
static void IRAM_ATTR encoder_isr(void *arg){
    (void)arg;
    s_encoder_count++;
}

// init encoder gpio
static void encoder_gpio_init(void){
    // changed to pull-up disable for testing CHANGE to enable for actual
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << ENCODER_GPIO,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_POSEDGE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));
    ESP_ERROR_CHECK(gpio_install_isr_service(0));
    ESP_ERROR_CHECK(gpio_isr_handler_add(ENCODER_GPIO, encoder_isr, NULL));
}

void app_main(void){
    esp_log_level_set("*", ESP_LOG_INFO);
    // turn on receiver
    espnow_receiver_init(1);
    // initialize encoder gpio 
    encoder_gpio_init();

    // init the PI
    static speed_pi_t s_pi;
    const float output_max = 1.0f;

    //start Ki small, raise Kp until response is stable.
    speed_pi_init(&s_pi, 0.5f, 0.05f, output_max);

    const TickType_t period = pdMS_TO_TICKS(100);
    
    // reset encoder count
    s_encoder_count = 0;
    // PI LOOP
    while (1) {
        float setpoint_m_s = 0.0f;
        int min = 0;
        int sec = 0;
        if (receiver_get_latest_pace(&min, &sec)) {
            // convert min and sec to meters/sec
            setpoint_m_s = speed_m_s_from_mile_pace_min_sec((uint16_t)min, (uint16_t)sec);
        }

        int32_t enc = s_encoder_count;
        int64_t t_us = esp_timer_get_time();
        float measured_m_s = 0.0f;
        // Return value = PI output to motor (0 … output_max(1)). 
        float pi_out = speed_pi_update(&s_pi, enc, t_us, setpoint_m_s, &measured_m_s);

        static int64_t last_log_us = 0;
        const int64_t log_period_us = 1000000; // log every 1 second
        if (t_us - last_log_us >= log_period_us) {
            last_log_us = t_us;
            ESP_LOGI(TAG,
                     "speed_pi_update out=%.4f | setpoint=%.3f m/s | meas=%.3f m/s | enc=%ld",
                     (double)pi_out, (double)setpoint_m_s, (double)measured_m_s, (long)enc);
        }
        vTaskDelay(period);
    }
}