/*
TODO:
- program flow
    - check updates
    - enter deep sleep
    - wakeup with button and display next
    - go back to sleep
    - wakeup periodically
- 
*/

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

#include "../components/epdiy/src/epd_highlevel.h"
#include "../components/epdiy/src/epdiy.h"
#include "../components/epdiy/src/render.h"

#define WAKE_UP_PERIOD_US (10 * 60 * 1000000)  // 60 seconds in microseconds

#define SLOT_SIZE (960*1024)
#define BYTES_TO_READ 100

void print_heap_info() {
    printf("Heap Information:\n");
    heap_caps_print_heap_info(MALLOC_CAP_DEFAULT);
}

void print_100_bytes(int slot) {
    // Find the partition named "images"
    const esp_partition_t *images_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x82, "images");
    
    if (images_partition == NULL) {
        ESP_LOGE("Partition", "Images partition not found");
        return;
    }

    // Calculate the offset based on the slot number
    size_t offset = slot * SLOT_SIZE;
    
    if (offset + BYTES_TO_READ > images_partition->size) {
        ESP_LOGE("Partition", "Slot offset exceeds partition size");
        return;
    }

    // Buffer to hold the 100 bytes
    uint8_t buffer[BYTES_TO_READ];

    // Read the 100 bytes from the calculated offset
    esp_err_t err = esp_partition_read(images_partition, offset, buffer, BYTES_TO_READ);

    if (err != ESP_OK) {
        ESP_LOGE("Partition", "Failed to read from partition: %s", esp_err_to_name(err));
        return;
    }

    // Print the 100 bytes as hexadecimal
    ESP_LOGI("Partition", "Data from slot %d:", slot);
    for (int i = 0; i < BYTES_TO_READ; i++) {
        printf("%02x ", buffer[i]);
    }
    printf("\n");
}




void enter_sleep() {
    ESP_LOGI("enter sleep:", "hello");
    sd_unmount();
    configure_rtc_gpio(BTN);
    configure_rtc_gpio(SD_EN);

   vTaskDelay(pdMS_TO_TICKS(200));

    esp_sleep_enable_ext0_wakeup(BTN, 0);
    //esp_sleep_enable_ext1_wakeup(1ULL << BTN, ESP_EXT1_WAKEUP_ALL_LOW);

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
    //renderer_init();

   // ota_init();
    //check_ota_updates();
    //vTaskDelay(pdMS_TO_TICKS(100));
    //send_device_info();
   //vTaskDelay(pdMS_TO_TICKS(2000));
    //update_voltages();

    server_sync();

    //

   // 
    //vTaskDelay(pdMS_TO_TICKS(100));

    //check_updates();


    vTaskDelay(pdMS_TO_TICKS(5000));

   //display_text("HELLO");

    enter_sleep();

    
   // 
    //
    //

    //vTaskDelay(pdMS_TO_TICKS(1000));
   // enter_sleep();
   
   

    //enter_sleep();

    
    //vTaskDelay(pdMS_TO_TICKS(1000));
    //vTaskDelay(pdMS_TO_TICKS(2000));

    //print_heap_info();
    //print_100_bytes(0);
    //ota_init();
    //vTaskDelay(pdMS_TO_TICKS(200));
    
}


void loop(){
    
    //disable_wifi();
    //vTaskDelay(pdMS_TO_TICKS(5000));
    //enter_sleep();
   vTaskDelay(pdMS_TO_TICKS(60 * 60 * 1000));
   // check_updates();
    //board_poweroff(&ctrl_state);
   // vTaskDelay(pdMS_TO_TICKS(10000));
    /*
    clear();
    display_image(1);
    vTaskDelay(pdMS_TO_TICKS(3000));
    clear();
    display_image(2);
    vTaskDelay(pdMS_TO_TICKS(4000));
    clear();
    clear();
   */
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

    while(1) {
       loop();
       //vTaskDelay(100000000);
    }
    
}

