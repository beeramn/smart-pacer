#include "get_mac.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "receiver.h"

void app_main(void)
{
    // This function gets the mac address (needed for ESP-NOW) of the esp box 
    // and prints to the monitor. 
    get_mac();

    //This function will listen for a pacemessage(2 ints) 
    // will send OK to transmitter 
    espnow_receiver_init(1);

}