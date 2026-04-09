#pragma once

#include <stdbool.h>
#include <stdint.h>

void espnow_receiver_init(uint8_t wifi_channel);
bool receiver_get_latest_pace(int *min_out, int *sec_out);