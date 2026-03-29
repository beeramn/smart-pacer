idf_component_register(
    SRCS "common_code.c" "get_mac.c" "receiver.c" "transmit.c" "ui.c"
    INCLUDE_DIRS "include"
    REQUIRES
        esp_wifi
        nvs_flash
        esp_event
        esp_netif)