#include "speedcontroller.h"
#include "esp_log.h"
#include <math.h>

// new variables for sliding window avg
#define SPEED_ESTIMATOR_WINDOW_US   (100000)   // 100 ms
#define SPEED_FILTER_ALPHA          (0.25f)    // 0..1, smaller = smoother

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
    // 20 ms control loop
    pi->prev_encoder_count = 0;
    pi->prev_time_us = 0;
    // 100 ms speed est timing
    pi->est_prev_encoder_count = 0;
    pi->est_prev_time_us = 0;
    pi->filtered_measured_m_s = 0.0f;

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
        
        pi->est_prev_encoder_count = encoder_count;
        pi->est_prev_time_us = time_us;
        pi->filtered_measured_m_s = 0.0f;

        pi->initialized = true;
        return 0.0f;
    }
    // control loop dt for intregral term
    int64_t dt_us = time_us - pi->prev_time_us;
    if (dt_us <= 0) {
        if (measured_m_s_out) {
            *measured_m_s_out = pi->filtered_measured_m_s;
        }
        return 0.0f;
    }
    float dt_s = (float)dt_us * 1e-6f;

    // update raw speed every 100 ms, between updates use last fileterd speed
    int64_t est_dt_us = time_us - pi->est_prev_time_us;
    if (est_dt_us >= SPEED_ESTIMATOR_WINDOW_US) {
        int32_t delta_counts = encoder_count - pi->est_prev_encoder_count;
        float delta_m = (float)delta_counts * SPEED_DISTANCE_PER_ENCODER_COUNT_M;
        float est_dt_s = (float)est_dt_us * 1e-6f;
        
        // calc speed
        float raw_measured_m_s = 0.0f;
        if (est_dt_s > 0.0f) {
            raw_measured_m_s = delta_m / est_dt_s;
        }
        // Low-pass filter
        pi->filtered_measured_m_s =
            SPEED_FILTER_ALPHA * raw_measured_m_s +
            (1.0f - SPEED_FILTER_ALPHA) * pi->filtered_measured_m_s;
        
        // update encoder and prev time pointers
        pi->est_prev_encoder_count = encoder_count;
        pi->est_prev_time_us = time_us;
    }
    // create new pointer for filtered speed
    if (measured_m_s_out) {
        *measured_m_s_out = pi->filtered_measured_m_s;
    }
    // PI uses filtered speed
    float error = setpoint_m_s - pi->filtered_measured_m_s;
    
    // if target is zero, reset integrator so it doesn't keep outputting
    // for starting and stoping --> integrate into main loop later 
    if (setpoint_m_s <= 0.0f) {
        pi->integral = 0.0f;
    } else {
        pi->integral += error * dt_s;
    }
    float output = pi->kp * error + pi->ki * pi->integral;

    // Clamp and anti-windup
    if (output > pi->output_max) {
        output = pi->output_max;
        if (pi->ki > 0.0f && error > 0.0f && setpoint_m_s > 0.0f) {
            pi->integral -= error * dt_s;
        }
    } else if (output < 0.0f) {
        output = 0.0f;
        if (pi->ki > 0.0f && error < 0.0f && setpoint_m_s > 0.0f) {
            pi->integral -= error * dt_s;
        }
    }

    // update pointer for encoder count and prev time
    pi->prev_encoder_count = encoder_count;
    pi->prev_time_us = time_us;

    return output;
}
