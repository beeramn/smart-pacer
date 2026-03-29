# How To Run 
- you might want to set up ur environment variables (MAC) `. ~/esp/esp-idf/export.sh`
- just like any other ESP project, cd into the main folder of either the esp for the car(receiver) or the controller (sender)
- `idf.py build`
- (MAC) Cmd + C: `ls /dev/cu.usb*` -> Output is the [PORT] 
- `idf.py -p [PORT] flash` Flashes code
- `idf.py -p [PORT] monitor` Monitor ESP output

## Functionality
- Right now when you have 2 ESPs connected and use the UO to set a pace and select [Start], it will send the selected pace to the receiver. 
- The receiver (Car ESP) just listens and sends an OK acknowledgment to transmiter
- once transmiter has receivev 3 of these, it stops sendings

## car_esp
- car esp script
(Noiluh) ESP MAC: 24:EC:4A:52:C3:64

## cont_esp
- controller esp script 
(Brandon) ESP Mac: 74:4D:BD:2E:08:24

## shared_components
- shared transmiter(controller esp sends message to car), receiver(car esp), and mac-reading scrips( prints the address of ESP {NEED FOR ESP NOW}).

## How to add shared functions
- if they're to be shared by both ESPs, put the `func.c` file in `/shared_components` and the corresponding `func.h` file in `/include`
- add the `func.c` file to the `CMakeLists.txt` in `shared_components` along the other files
- when using in ESP folders remmeber to add `#include` to the top of the file. EX: `#include "get_mac.h"` 


### transmiter.c 
- has all the transmitting functions, At the moment will keep transmitting until it receives 3 OK messages from receiver

### receiver.c
- RIGHT NOW: Only listens for messages and sends OK back to transmiter when it gets something

### ui.c
- has (mostly) all the functions from flavi's original script. Used in cont_esp.c to create the interface and send pace to receiver
