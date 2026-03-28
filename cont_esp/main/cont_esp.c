#include "get_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "transmit.h"
void app_main(void)
{
    // This function gets the mac address (needed for ESP-NOW) of the esp box 
    // and prints to the monitor. 
    get_mac();

    //This function will transmit a message(2 ints) 
    // to the mac address of car esp
    uint8_t receiver_mac[ESP_NOW_ETH_ALEN] = {
        0x24, 0xEC, 0x4A, 0x52, 0xC3, 0x64
    };

    espnow_transmit_init(1); // channel 

    vTaskDelay(pdMS_TO_TICKS(1000));

    transmit_two_ints(receiver_mac, 7, 49); // pace message 

}