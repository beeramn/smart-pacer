#ifndef LINE_FOLLOW_H
#define LINE_FOLLOW_H

#include <stdbool.h>
#include <stdint.h>
#include "driver/gpio.h"

// ================= PIN MAPPING =================
#define EN_PIN GPIO_NUM_41 // TODO tie to high
#define SERVO_GPIO GPIO_NUM_38

extern const gpio_num_t sensorPins[8];

// ================ LINE SENSOR CONFIG ================
extern const bool LINE_IS_BLACK;

// left negative, right positive
extern const int weights[8];

// ================ SERVO PWM CONFIG ==================
#define PWM_FREQ_HZ        50
#define SERVO_CENTER_US    1500
#define SERVO_LEFT_US      1000
#define SERVO_RIGHT_US     2000

extern uint32_t lastPulseUs;

// ================= FUNCTION PROTOTYPES =================
uint32_t us_to_duty(uint32_t pulse_us);
void set_pulse_us(uint32_t pulse_us);
int map_int(int x, int in_min, int in_max, int out_min, int out_max);
int constrain_int(int x, int min_val, int max_val);

void init_servo(void);
void init_line_sensors(void);

#endif