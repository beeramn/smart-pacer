// // #include "get_mac.h"
// // #include "freertos/FreeRTOS.h"
// // #include "freertos/task.h"
// // #include "receiver.h"

// // void app_main(void)
// // {
// //     // This function gets the mac address (needed for ESP-NOW) of the esp box 
// //     // and prints to the monitor. 
// //     get_mac();

// //     //This function will listen for a pacemessage(2 ints) 
// //     // will send OK to transmitter 
// //     espnow_receiver_init(1);

// // }


// #include <stdio.h>
// #include "freertos/FreeRTOS.h"
// #include "freertos/task.h"
// #include "driver/ledc.h"
// #include "esp_err.h"

// #define SERVO_GPIO         13
// #define LEDC_TIMER         LEDC_TIMER_0
// #define LEDC_MODE          LEDC_LOW_SPEED_MODE
// #define LEDC_CHANNEL       LEDC_CHANNEL_0
// #define PWM_FREQ_HZ        50
// #define PWM_RESOLUTION     LEDC_TIMER_14_BIT
// #define DUTY_MAX           ((1 << 14) - 1)

// static uint32_t us_to_duty(uint32_t pulse_us)
// {
//     const uint32_t period_us = 20000; // 20 ms -> 50 Hz
//     return (pulse_us * DUTY_MAX) / period_us;
// }

// static void set_pulse_us(uint32_t pulse_us)
// {
//     uint32_t duty = us_to_duty(pulse_us);
//     ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
//     ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
// }

// static void sweep_servo(uint32_t start_us, uint32_t end_us, uint32_t step_us, uint32_t delay_ms)
// {
//     if (start_us < end_us) {
//         for (uint32_t p = start_us; p <= end_us; p += step_us) {
//             set_pulse_us(p);
//             vTaskDelay(pdMS_TO_TICKS(delay_ms));
//         }
//     } else {
//         for (int p = (int)start_us; p >= (int)end_us; p -= (int)step_us) {
//             set_pulse_us((uint32_t)p);
//             vTaskDelay(pdMS_TO_TICKS(delay_ms));
//         }
//     }
// }

// void app_main(void)
// {
//     ledc_timer_config_t timer_conf = {
//         .speed_mode = LEDC_MODE,
//         .timer_num = LEDC_TIMER,
//         .duty_resolution = PWM_RESOLUTION,
//         .freq_hz = PWM_FREQ_HZ,
//         .clk_cfg = LEDC_AUTO_CLK
//     };
//     ESP_ERROR_CHECK(ledc_timer_config(&timer_conf));

//     ledc_channel_config_t ch_conf = {
//         .gpio_num = SERVO_GPIO,
//         .speed_mode = LEDC_MODE,
//         .channel = LEDC_CHANNEL,
//         .timer_sel = LEDC_TIMER,
//         .duty = 0,
//         .hpoint = 0
//     };
//     ESP_ERROR_CHECK(ledc_channel_config(&ch_conf));

//     vTaskDelay(pdMS_TO_TICKS(1000));

//     // Start centered
//     set_pulse_us(1500);
//     vTaskDelay(pdMS_TO_TICKS(1000));

//     while (1) {
//         sweep_servo(1500, 1000, 5, 20);
//         vTaskDelay(pdMS_TO_TICKS(300));

//         sweep_servo(1000, 2000, 5, 20);
//         vTaskDelay(pdMS_TO_TICKS(300));

//         sweep_servo(2000, 1500, 5, 20);
//         vTaskDelay(pdMS_TO_TICKS(300));
//     }
// }

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/ledc.h"
#include "driver/gpio.h"
#include "esp_err.h"

// ================= PIN MAPPING =================
// Assumes common ESP32 DevKit / WROOM board

#define EN_PIN GPIO_NUM_41

static const gpio_num_t sensorPins[8] = {
    GPIO_NUM_12,
    GPIO_NUM_11,
    GPIO_NUM_14,
    GPIO_NUM_10,
    GPIO_NUM_42,
    GPIO_NUM_13,
    GPIO_NUM_9,
    GPIO_NUM_21
};

#define SERVO_GPIO GPIO_NUM_38

// ================ LINE SENSOR CONFIG ================
static const bool LINE_IS_BLACK = true;

// left negative, right positive
static const int weights[8] = {-350, -250, -150, -50, 50, 150, 250, 350};

// ================ SERVO PWM CONFIG ==================
#define LEDC_TIMER         LEDC_TIMER_0
#define LEDC_MODE          LEDC_LOW_SPEED_MODE
#define LEDC_CHANNEL       LEDC_CHANNEL_0
#define PWM_FREQ_HZ        50
#define PWM_RESOLUTION     LEDC_TIMER_14_BIT
#define DUTY_MAX           ((1 << 14) - 1)

// Tune if needed
#define SERVO_CENTER_US    1500
#define SERVO_LEFT_US      1000 // used to be 1200
#define SERVO_RIGHT_US     2000 // used to be 1800

static uint32_t lastPulseUs = SERVO_CENTER_US;

// ================= HELPER FUNCTIONS =================
static uint32_t us_to_duty(uint32_t pulse_us)
{
    const uint32_t period_us = 20000; // 20 ms for 50 Hz
    return (pulse_us * DUTY_MAX) / period_us;
}

static void set_pulse_us(uint32_t pulse_us)
{
    uint32_t duty = us_to_duty(pulse_us);
    ESP_ERROR_CHECK(ledc_set_duty(LEDC_MODE, LEDC_CHANNEL, duty));
    ESP_ERROR_CHECK(ledc_update_duty(LEDC_MODE, LEDC_CHANNEL));
}

static int map_int(int x, int in_min, int in_max, int out_min, int out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static int constrain_int(int x, int min_val, int max_val)
{
    if (x < min_val) return min_val;
    if (x > max_val) return max_val;
    return x;
}

static void init_servo(void)
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
        .intr_type = LEDC_INTR_DISABLE,
        .timer_sel = LEDC_TIMER,
        .duty = 0,
        .hpoint = 0
    };
    ESP_ERROR_CHECK(ledc_channel_config(&ch_conf));

    vTaskDelay(pdMS_TO_TICKS(1000));
    set_pulse_us(SERVO_CENTER_US);
    vTaskDelay(pdMS_TO_TICKS(1000));
}

static void init_line_sensors(void)
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

void app_main(void)
{
    init_line_sensors();
    init_servo();

    while (1) {
        int weightedSum = 0;
        int activeCount = 0;

        printf("Sensors: ");

        for (int i = 0; i < 8; i++) {
            int raw = gpio_get_level(sensorPins[i]);

            // convert to "line detected" = 1
            int onLine = LINE_IS_BLACK ? raw : !raw;

            printf("%d ", onLine);

            if (onLine) {
                weightedSum += weights[i];
                activeCount++;
            }
        }

        if (activeCount == 0) {
            printf(" -> line lost");
            set_pulse_us(lastPulseUs);
            printf(" -> servo pulse: %lu\n", (unsigned long)lastPulseUs);
        } else {
            int error = weightedSum / activeCount; // about -350 to +350

            printf(" -> error: %d", error);

            int pulseUs = map_int(error, -350, 350, SERVO_LEFT_US, SERVO_RIGHT_US);
            pulseUs = constrain_int(pulseUs, SERVO_LEFT_US, SERVO_RIGHT_US);

            // optional smoothing
            pulseUs = (3 * (int)lastPulseUs + pulseUs) / 4;

            set_pulse_us((uint32_t)pulseUs);
            lastPulseUs = (uint32_t)pulseUs;

            printf(" -> servo pulse: %d -> ", pulseUs);

            if (error < -180) {
                printf("turn LEFT hard\n");
            } else if (error < -60) {
                printf("turn LEFT\n");
            } else if (error > 180) {
                printf("turn RIGHT hard\n");
            } else if (error > 60) {
                printf("turn RIGHT\n");
            } else {
                printf("STRAIGHT\n");
            }
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}




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