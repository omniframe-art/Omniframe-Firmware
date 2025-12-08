#include "image_data.h"
#include "image_queue.h"
#include "esp_log.h"
#include "float.h"
#include "esp_heap_caps.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include <esp_sleep.h>
#include "driver/gpio.h"
#include "driver/adc.h"
#include "esp_partition.h"


#include "config.h"
#include "wifi.h"
#include "ota.h"
#include "api.h"
#include "download.h"
#include "button.h"
#include "sd_card.h"
#include "measure.h"
#include "text.h"

#define WAKE_UP_PERIOD_US (10 * 60 * 1000000)  // 60 seconds in microseconds

#define SLOT_SIZE (960*1024)
#define BYTES_TO_READ 100

void print_heap_info() {
    printf("Heap Information:\n");
    heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
}

void enter_sleep() {
    ESP_LOGI("enter sleep:", "hello");
    sd_unmount();
    configure_rtc_gpio(BTN);
    configure_rtc_gpio(SD_EN);

    vTaskDelay(pdMS_TO_TICKS(200));

    esp_sleep_enable_ext0_wakeup(BTN, 0);
    esp_sleep_enable_timer_wakeup(WAKE_UP_PERIOD_US);
    esp_deep_sleep_start();
}

void setup(){
    deconfigure_rtc_gpio(SD_EN);
    nvs_init();
    button_init();
    board_init();
    measure_init();
    sd_init();

    setup_wifi();
    vTaskDelay(pdMS_TO_TICKS(500));
    get_queue_nvs();
    get_waveform();
    renderer_init();

    //ota_init();
    //check_ota_updates();
    //vTaskDelay(pdMS_TO_TICKS(100));
    
    //send_device_info(); //needs to only be done once

    server_sync();

    vTaskDelay(pdMS_TO_TICKS(5000));

    enter_sleep();    
}


void app_main() {
    // Check wakeup cause
    esp_sleep_wakeup_cause_t wakeup_reason = esp_sleep_get_wakeup_cause();

    switch (wakeup_reason) {

        case ESP_SLEEP_WAKEUP_TIMER:
            // Woke up from timer (periodic wake)
            ESP_LOGI("main", "Wakeup caused by timer");
            setup();
            break;

        default:
            // First boot or wakeup from other sources
            ESP_LOGI("main", "First boot or wakeup from other source");
            setup();
            break;
    }
    
}

