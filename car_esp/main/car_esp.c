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
// Motor GPIO stuff
#define MOTOR_RPWM_GPIO   14
#define MOTOR_EN_GPIO    16 // connect both enables to this pin, both must be on
// LRPM pin we can connect to ground, we can also just connect enables directly to power

#define MOTOR_LEDC_TIMER       LEDC_TIMER_0
#define MOTOR_LEDC_MODE        LEDC_LOW_SPEED_MODE
#define MOTOR_LEDC_RPWM_CH     LEDC_CHANNEL_0

#define MOTOR_PWM_FREQ_HZ      20000
#define MOTOR_PWM_RESOLUTION   LEDC_TIMER_10_BIT
#define MOTOR_PWM_DUTY_MAX     ((1 << 10) - 1)


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
// init motor driver
static void motor_driver_init(void){
    // Enable pins as normal GPIO outputs
    gpio_config_t io = {
        .pin_bit_mask = 1ULL << MOTOR_EN_GPIO,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io));

    // Turn on both BTS7960 enable pins
    ESP_ERROR_CHECK(gpio_set_level(MOTOR_EN_GPIO, 1));

    // Configure PWM timer
    ledc_timer_config_t tcfg = {
        .speed_mode = MOTOR_LEDC_MODE,
        .duty_resolution = MOTOR_PWM_RESOLUTION,
        .timer_num = MOTOR_LEDC_TIMER,
        .freq_hz = MOTOR_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    ESP_ERROR_CHECK(ledc_timer_config(&tcfg));

    // RPWM channel (forward drive)
    ledc_channel_config_t rpwm = {
        .gpio_num = MOTOR_RPWM_GPIO,
        .speed_mode = MOTOR_LEDC_MODE,
        .channel = MOTOR_LEDC_RPWM_CH,
        .timer_sel = MOTOR_LEDC_TIMER,
        .duty = 0,
        .hpoint = 0,
    };
    ESP_ERROR_CHECK(ledc_channel_config(&rpwm));

}
// function for PWM signal
static void motor_set_forward_from_pi(float cmd){
    if (cmd < 0.0f) cmd = 0.0f;
    if (cmd > 1.0f) cmd = 1.0f;

    uint32_t duty = (uint32_t)(cmd * MOTOR_PWM_DUTY_MAX);

    ledc_set_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_RPWM_CH, duty);
    ledc_update_duty(MOTOR_LEDC_MODE, MOTOR_LEDC_RPWM_CH);
}


void app_main(void){
    esp_log_level_set("*", ESP_LOG_INFO);
    // turn on receiver
    espnow_receiver_init(1);
    // initialize encoder gpio 
    encoder_gpio_init();
    // initialize motor driver init
    motor_driver_init();

    // init the PI
    static speed_pi_t s_pi;
    const float output_max = 1.0f;

    //start Ki small, raise Kp until response is stable.
    speed_pi_init(&s_pi, 0.5f, 0.05f, output_max);
    // how often we read encoder, but sliding window for 100 ms
    const TickType_t period = pdMS_TO_TICKS(20);
    
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
        // send PI output to motor 
        motor_set_forward_from_pi(pi_out);
        
        // log every loop
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