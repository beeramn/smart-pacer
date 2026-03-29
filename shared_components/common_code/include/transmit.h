#ifndef TRANSMIT_H
#define TRANSMIT_H

#include <stdint.h>
#include "esp_now.h"

typedef struct {
    int value1;
    int value2;
} espnow_int_msg_t;

void espnow_transmit_init(uint8_t wifi_chennel);
void transmit_two_ints(const uint8_t peer_mac[ESP_NOW_ETH_ALEN], int value1, int value2, volatile bool *stop_requested);
#endif