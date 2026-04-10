#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_err.h"

#include "linefollower.h"

static const ledc_timer_t LEDC_TIMER_USED = LEDC_TIMER_0;
static const ledc_mode_t LEDC_MODE_USED = LEDC_LOW_SPEED_MODE;
static const ledc_channel_t LEDC_CHANNEL_USED = LEDC_CHANNEL_0;
static const ledc_timer_bit_t PWM_RESOLUTION = LEDC_TIMER_14_BIT;
static const uint32_t DUTY_MAX = ((1 << 14) - 1);

const gpio_num_t sensorPins[8] = {
    GPIO_NUM_12,
    GPIO_NUM_11,
    GPIO_NUM_14,
    GPIO_NUM_10,
    GPIO_NUM_42,
    GPIO_NUM_13,
    GPIO_NUM_9,
    GPIO_NUM_21
};

const bool LINE_IS_BLACK = true;

// left negative, right positive
const int weights[8] = {-350, -250, -150, -50, 50, 150, 250, 350};

uint32_t lastPulseUs = SERVO_CENTER_US;

uint32_t us_to_duty(uint32_t pulse_us)
{
    const uint32_t period_us = 20000; // 20 ms for 50 Hz
    return (pulse_us * DUTY_MAX) / period_us;
}

void set_pulse_us(uint32_t pulse_us)
{
    uint32_t duty = us_to_duty(pulse_us);
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE_USED, LEDC_CHANNEL_USED, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE_USED, LEDC_CHANNEL_USED));
}

int map_int(int x, int in_min, int in_max, int out_min, int out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

int constrain_int(int x, int min_val, int max_val)
{
    if (x < min_val) return min_val;
    if (x > max_val) return max_val;
    return x;
}

void init_servo(void)
{
    ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_MODE_USED,
        .timer_num = LEDC_TIMER_USED,
        .duty_resolution = PWM_RESOLUTION,
        .freq_hz = PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK
    };
    ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

    ledc_channel_config_t ch_conf = {
        .gpio_num = SERVO_GPIO,
        .speed_mode = LEDC_MODE_USED,
        .channel = LEDC_CHANNEL_USED,
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER_USED,
        .duty = 0,
        .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_conf));

    vTaskDelay(pdMS_TO_TICKS(1000));
    set_pulse_us(SERVO_CENTER_US);
    vTaskDelay(pdMS_TO_TICKS(1000));
}

void init_line_sensors(void)
{
    gpio_config_t en_conf = {
        .pin_bit_mask = (1ULL << EN_PIN),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&en_conf));
    ESP_ERROR_CHECK(gpio_set_level(EN_PIN, 1)); // enable sensor board

    uint64_t sensor_mask = 0;
    for (int i = 0; i < 8; i++) {
        sensor_mask |= (1ULL << sensorPins[i]);
    }

    gpio_config_t sensor_conf = {
        .pin_bit_mask = sensor_mask,
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE
    };
    ESP_ERROR_CHECK(gpio_config(&sensor_conf));
}