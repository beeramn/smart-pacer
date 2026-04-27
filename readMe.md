# Smart Pace-Setter Line Following Car

An IoT project that implements a closed-loop, line-following RC car designed to maintain a consistent pace along a track. The system integrates real-time sensing, control algorithms, and optional cloud-based data logging.

## Overview

The Smart Pace-Setter Car follows a predefined track using a reflectance sensor array while maintaining a steady speed. Unlike traditional line-following robots, this system emphasizes consistent pacing, making it suitable for applications such as athletic training or rehabilitation.

Key components:
- Embedded systems (ESP32)
- Real-time control (PI controller and steering control)
- Sensor processing (QTI array)
- Networking (HTTP and cloud backend)

## Features

- Line following using an 8-sensor QTI array with weighted error calculation
- Closed-loop steering control using PWM-controlled servo
- Steering smoothing to reduce jitter
- Speed regulation using PI control with encoder feedback
- IoT data logging via HTTP POST to a cloud backend (proof of concept)
- Real-time operation using ESP-IDF and FreeRTOS

## Hardware Components

- ESP32-S3 microcontroller
- Parallax 8-QTI sensor array
- RS-550 DC motor
- BTS7960 motor driver
- Steering servo motor
- Wheel encoder
- Battery and buck coverter

## Software Stack

Embedded:
- ESP-IDF (v5.x)
- FreeRTOS
- esp_http_client
- LEDC (PWM control)
- LVGL (optional UI)

Backend:
- Python Flask server
- PostgreSQL database
- psycopg2

## Challenges

- Sensor calibration due to sensitivity to lighting conditions
- High current draw from RS-550 motor
- Integration of sensing, control, and networking
- Mechanical limitations of the RC platform

## Future Work

- Replace QTI sensors with camera-based vision
- Improve robustness to lighting variation
- Develop wearable IMU-based pacing system
- Add data visualization and analytics
- Improve motor control and power efficiency

## Getting Started

Build:
idf.py build

Flash:
idf.py -p <PORT> flash monitor

## Contributors

- Flaviana Keller
- Brandon Ramirez
- Neyla Kirby