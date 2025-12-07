# Source files organized by domain
set(app_sources 
    "main.c"
    "config.c"
    # Display
    "display/image_data.c"
    "display/image_queue.c"
    "display/text.c"
    # Network
    "network/wifi.c"
    "network/api.c"
    "network/download.c"
    "network/ota.c"
    # Drivers
    "drivers/tps.c"
    "drivers/button.c"
    "drivers/sd_card.c"
    "drivers/measure.c"
)

set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,-Map=output.map")

# Register the component with these source files and specify its dependencies.
idf_component_register(SRCS ${app_sources}
                      INCLUDE_DIRS "." "display" "network" "drivers"
                      REQUIRES epdiy esp_wifi nvs_flash esp_http_client app_update esp_partition json mbedtls fatfs)