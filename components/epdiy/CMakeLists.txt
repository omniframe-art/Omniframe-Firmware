
set(app_sources "src/epdiy.c"
                "src/render.c"
                "src/output_lcd/render_lcd.c"
                "src/output_lcd/lcd_driver.c"
                "src/output_common/lut.c"
                "src/output_common/line_queue.c"
                "src/output_common/render_context.c"
                "src/highlevel.c"
                "src/board/tps65185.c"
                "src/board/pca9555.c"
                "src/board/epd_board.c"
                "src/board/epd_board_common.c"
                "src/board/epd_board_v7.c"
)


# Can also use IDF_VER for the full esp-idf version string but that is harder to parse. i.e. v4.1.1, v5.0-beta1, etc
if (${IDF_VERSION_MAJOR} GREATER 4)
    idf_component_register(SRCS ${app_sources} INCLUDE_DIRS "src/" REQUIRES driver esp_timer esp_adc esp_lcd esp_partition)
else()
    idf_component_register(SRCS ${app_sources} INCLUDE_DIRS "src/" REQUIRES esp_adc_cal esp_timer esp_lcd esp_partition)
endif()

# formatting specifiers maybe incompatible between idf versions because of different int definitions
component_compile_options(-Wno-error=format= -Wno-format)
set_source_files_properties("src/output_common/lut.c" PROPERTIES COMPILE_OPTIONS -mno-fix-esp32-psram-cache-issue)
