// ********************************************************
// ********************************************************
// THIS FILE GIVES THE STA MAC ADDRESS OF THE ESP DEVICE 
// NEED TS FOR ESPNOW
// ********************************************************
// ********************************************************

#include <stdio.h>
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_log.h"
#include "get_mac.h"


void get_mac(void)
{
    uint8_t mac[6];

    esp_err_t err = esp_read_mac(mac, ESP_MAC_WIFI_STA);
    if (err != ESP_OK) {
        printf("Failed to read MAC address: %s\n", esp_err_to_name(err));
        return;
    }

    printf("ESP32-box3 MAC (WiFi STA): %02X:%02X:%02X:%02X:%02X:%02X\n",
           mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
}