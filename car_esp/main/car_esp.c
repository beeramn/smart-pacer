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
#include "esp_err.h"

#define SERVO_GPIO         13
#define LEDC_TIMER         LEDC_TIMER_0
#define LEDC_MODE          LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL       LEDC_CHANNEL_0
#define PWM_FREQ_HZ        50
#define PWM_RESOLUTION     LEDC_TIMER_14_BIT
#define DUTY_MAX           ((1 << 14) - 1)

static uint32_t us_to_duty(uint32_t pulse_us)
{
    const uint32_t period_us = 20000; // 20 ms -> 50 Hz
    return (pulse_us * DUTY_MAX) / period_us;
}

static void set_pulse_us(uint32_t pulse_us)
{
    uint32_t duty = us_to_duty(pulse_us);
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

static void sweep_servo(uint32_t start_us, uint32_t end_us, uint32_t step_us, uint32_t delay_ms)
{
    if (start_us < end_us) {
        for (uint32_t p = start_us; p <= end_us; p += step_us) {
            set_pulse_us(p);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    } else {
        for (int p = (int)start_us; p >= (int)end_us; p -= (int)step_us) {
            set_pulse_us((uint32_t)p);
            vTaskDelay(pdMS_TO_TICKS(delay_ms));
        }
    }
}

void app_main(void)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_MODE,
        .timer_num = LEDC_TIMER,
        .duty_resolution = PWM_RESOLUTION,
        .freq_hz = PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

    ledc_channel_config_t ch_conf = {
        .gpio_num = SERVO_GPIO,
        .speed_mode = LEDC_MODE,
        .channel = LEDC_CHANNEL,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_conf));

    vTaskDelay(pdMS_TO_TICKS(1000));

    // Start centered
    set_pulse_us(1500);
    vTaskDelay(pdMS_TO_TICKS(1000));

    while (1) {
        sweep_servo(1500, 1000, 5, 20);
        vTaskDelay(pdMS_TO_TICKS(300));

        sweep_servo(1000, 2000, 5, 20);
        vTaskDelay(pdMS_TO_TICKS(300));

        sweep_servo(2000, 1500, 5, 20);
        vTaskDelay(pdMS_TO_TICKS(300));
    }
}