idf_component_register(SRCS "car_esp.c" "speedcontroller.c"
                    INCLUDE_DIRS "."
                    REQUIRES common_code driver esp_timer)
