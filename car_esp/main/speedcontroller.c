#include "speedcontroller.h"

#include <math.h>

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
                      float setpoint_m_s, float *measured_m_s_out)
{
    if (!pi) {
        return 0.0f;
    }

    if (measured_m_s_out) {
        *measured_m_s_out = 0.0f;
    }

    if (!pi->initialized) {
        pi->prev_encoder_count = encoder_count;
        pi->prev_time_us = time_us;
        pi->initialized = true;
        return 0.0f;
    }

    int64_t dt_us = time_us - pi->prev_time_us;
    if (dt_us <= 0) {
        float p = pi->kp * setpoint_m_s;
        if (p > pi->output_max) {
            return pi->output_max;
        }
        return p;
    }

    int32_t delta_counts = encoder_count - pi->prev_encoder_count;
    float delta_m = (float)delta_counts * SPEED_DISTANCE_PER_ENCODER_COUNT_M;
    float dt_s = (float)dt_us * 1e-6f;
    float measured_m_s = delta_m / dt_s;
    if (measured_m_s < 0.0f) {
        measured_m_s = 0.0f;
    }

    if (measured_m_s_out) {
        *measured_m_s_out = measured_m_s;
    }

    float error = setpoint_m_s - measured_m_s;
    pi->integral += error * dt_s;
    float output = pi->kp * error + pi->ki * pi->integral;

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

    pi->prev_encoder_count = encoder_count;
    pi->prev_time_us = time_us;

    return output;
}
