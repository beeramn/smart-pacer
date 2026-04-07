#pragma once

#include <stdbool.h>
#include <stdint.h>

#define SPEED_WHEEL_DIAMETER_M (0.115f)
/** One revolution: π * D [m] */
#define SPEED_WHEEL_CIRCUMFERENCE_M (3.14159265358979323846f * SPEED_WHEEL_DIAMETER_M)
/** Magnetic wheel: 4 N + 4 S poles → 8 encoder counts per wheel revolution */
#define SPEED_ENCODER_COUNTS_PER_WHEEL_REV 8
/** Distance traveled for one encoder count [m] */
#define SPEED_DISTANCE_PER_ENCODER_COUNT_M \
    (SPEED_WHEEL_CIRCUMFERENCE_M / (float)SPEED_ENCODER_COUNTS_PER_WHEEL_REV)

/**
 * Forward-only + BTS7960: PI output is 0…output_max (e.g. PWM duty fraction).
 * Hold one direction fixed (RPWM + enables, LPWM low — per your breakout’s forward table).
 */

typedef struct {
    float kp;
    float ki;
    float integral;
    int32_t prev_encoder_count;
    int64_t prev_time_us;
    float output_max;
    bool initialized;
} speed_pi_t;

void speed_pi_init(speed_pi_t *pi, float kp, float ki, float output_max);

/**
 * setpoint_m_s: desired forward speed [m/s] (caller keeps this ≥ 0).
 * encoder_count: cumulative count; reverse motion yields 0 measured speed for PI.
 * measured_m_s_out: optional non-negative estimated forward speed over the last interval.
 * Returns PI output in [0, output_max].
 */
float speed_pi_update(speed_pi_t *pi, int32_t encoder_count, int64_t time_us,
                      float setpoint_m_s, float *measured_m_s_out);
