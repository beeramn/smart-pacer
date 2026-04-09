#include "speedcontroller.h"
#include "esp_log.h"
#include <math.h>
// call this in car_esp.c to get setpoint_ms like this: float setpoint_m_s = speed_m_s_from_mile_pace_min_sec(msg.value1, msg.value2);
float speed_m_s_from_mile_pace_min_sec(uint16_t minutes, uint16_t seconds){
    float total_s = (float)minutes * 60.0f + (float)seconds;
    if (total_s <= 0.0f) {
        return 0.0f;
    }
    // speed = distance / time
    return SPEED_MILE_DISTANCE_M / total_s;
}

void speed_pi_init(speed_pi_t *pi, float kp, float ki, float output_max)
{
    if (!pi) {
        return;
    }
    pi->kp = kp;
    pi->ki = ki;
    pi->integral = 0.0f;
    pi->prev_encoder_count = 0;
    pi->prev_time_us = 0;
    pi->output_max = (output_max > 0.0f) ? output_max : 1.0f;
    pi->initialized = false;
}

float speed_pi_update(speed_pi_t *pi, int32_t encoder_count, int64_t time_us,
                      float setpoint_m_s, float *measured_m_s_out){
    if (!pi) {
        return 0.0f;
    }

    if (measured_m_s_out) {
        *measured_m_s_out = 0.0f;
    }

    // First sample: initialize controller
    if (!pi->initialized) {
        pi->prev_encoder_count = encoder_count;
        pi->prev_time_us = time_us;
        pi->initialized = true;
        return 0.0f;
    }

    int64_t dt_us = time_us - pi->prev_time_us;
    // Equal timestamp: proportional-only guess
    if (dt_us <= 0) {
        float p = pi->kp * setpoint_m_s;
        if (p > pi->output_max) {
            return pi->output_max;
        }
        return p;
    }

    // Calculate speed from count delta since last call.
    int32_t delta_counts = encoder_count - pi->prev_encoder_count;
    float delta_m = (float)delta_counts * SPEED_DISTANCE_PER_ENCODER_COUNT_M;
    float dt_s = (float)dt_us * 1e-6f;
    float measured_m_s = delta_m / dt_s;
    // Forward-only: negative delta → treat speed as 0 for the loop.
    if (measured_m_s < 0.0f) {
        measured_m_s = 0.0f;
    }

    if (measured_m_s_out) {
        *measured_m_s_out = measured_m_s;
    }

    float error = setpoint_m_s - measured_m_s;
    pi->integral += error * dt_s;
    float output = pi->kp * error + pi->ki * pi->integral;

    // Clamp and anti-windup
    if (output > pi->output_max) {
        output = pi->output_max;
        if (pi->ki > 0.0f && error > 0.0f) {
            pi->integral -= error * dt_s;
        }
    } else if (output < 0.0f) {
        output = 0.0f;
        if (pi->ki > 0.0f && error < 0.0f) {
            pi->integral -= error * dt_s;
        }
    }
    // log measured since rn we get 0. 
    //ESP_LOGI("SC", "meas=%.6f", (double)measured_m_s);
    // update pointer for encoder count and prev time
    pi->prev_encoder_count = encoder_count;
    pi->prev_time_us = time_us;

    return output;
}
