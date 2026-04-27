# Smart Pace-Setter Line Following Car

An embedded systems project that implements a closed-loop, line-following RC car designed to maintain a consistent pace along a track. The system integrates real-time sensing, control algorithms, and optional cloud-based data logging.

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

## System Architecture

QTI Sensors -> Error Calculation -> Steering Control (Servo PWM)
                              |
                              v
                    Speed Control (PI + Encoder)
                              |
                              v
                        Motor Driver
                              |
                              v
                          Car Motion
                              |
                              v
        (Optional) HTTP POST -> Flask Server -> PostgreSQL

## Hardware Components

- ESP32-S3 microcontroller
- Parallax 8-QTI sensor array
- RS-550 DC motor
- BTS7960 motor driver
- Steering servo motor
- Wheel encoder
- Battery and 3.3V regulator

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

## Control Algorithm

Line Following:
Each QTI sensor is assigned a positional weight:
[-350, -250, -150, -50, +50, +150, +250, +350]

Error is computed as the weighted sum of active sensors:
- Negative error -> steer left
- Positive error -> steer right

Steering Smoothing:
pulseUs = (3 * (int)lastPulseUs + pulseUs) / 4;

This acts as a low-pass filter to smooth control signals.

Speed Control:
- PI controller uses encoder feedback to maintain target velocity
- Motor input is adjusted based on speed error

## Data Logging (Optional)

ESP32 sends JSON via HTTP POST:
{
  "start_time": "...",
  "end_time": "...",
  "pace": "..."
}

Flask endpoint:
POST /sensor

Database fields:
- start_time
- end_time
- pace

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

## License

This project is for educational purposes. Add a license if distributing publicly.