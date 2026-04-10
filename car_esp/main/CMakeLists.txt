idf_component_register(SRCS "line_following.c" "car_esp.c" "speedcontroller.c"
                    INCLUDE_DIRS "."
                    REQUIRES common_code driver esp_timer)
