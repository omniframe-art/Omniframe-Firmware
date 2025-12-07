#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"
#include "esp_log.h"
#include "config.h"
#include "ota.h"
#include "image_data.h"
#include "button.h"
#include "api.h"
#include "wifi.h"

#define TAG "BUTTON"

#define DEBOUNCE_TIME 50
#define MAX_PRESS_INTERVAL 600
#define BUTTON_TASK_STACK_SIZE 4096
#define LONG_PRESS_TIME_3S 1500
#define LONG_PRESS_TIME_6S 4000

static QueueHandle_t gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void button_task(void* arg)
{
    uint32_t io_num;
    uint32_t press_count = 0;
    TickType_t press_start_time = 0;

    for (;;) {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            vTaskDelay(pdMS_TO_TICKS(DEBOUNCE_TIME));

            if (gpio_get_level(io_num) == 0) { // Button pressed
                press_start_time = xTaskGetTickCount();
                uint32_t press_duration = 0;
                bool double_press_detected = false; // Flag for double press
                
                // Wait while button is held
                while (gpio_get_level(io_num) == 0) {
                    TickType_t current_time = xTaskGetTickCount();
                    press_duration = pdTICKS_TO_MS(current_time - press_start_time);
                    vTaskDelay(pdMS_TO_TICKS(10));
                }

                ESP_LOGI(TAG, "Button released after %u ms", (unsigned int)press_duration);
                if (press_duration >= LONG_PRESS_TIME_6S) {
                    process_button_press(BUTTON_PRESS_6S);
                } else if (press_duration >= LONG_PRESS_TIME_3S) {
                    process_button_press(BUTTON_PRESS_3S);
                } else {
                    // Wait for potential additional presses within MAX_PRESS_INTERVAL
                    TickType_t wait_start = xTaskGetTickCount();
                    
                    while ((xTaskGetTickCount() - wait_start) < pdMS_TO_TICKS(MAX_PRESS_INTERVAL)) {
                        if (gpio_get_level(io_num) == 0) {
                            double_press_detected = true;
                            process_button_press(BUTTON_DOUBLE_PRESS);
                            break;
                        }
                        vTaskDelay(pdMS_TO_TICKS(10));
                    }
                    if (!double_press_detected) {
                        process_button_press(BUTTON_SINGLE_PRESS); // Single press if no double press detected
                    }
                }
            }
        }
    }
}

int demo_current = 0;
char demo_queue[10][256] = {
    "Joni_Moilanen^Fly_Away.bin",
    "Antonietta_Positano^Disc_3.2.bin",
    //"CPL^unnamed_1.1.bin",
    "Escher^Relativity.bin",
    //"Salvador_Dali^Sketches.bin",
    //"Ankita_Bhattacharya^Insoluble.bin",
    //"Sofia_Seguro^4.bin",
    "Escher^Print_Gallery.bin",
    "Morris^.bin",
    "Rivail^1.1.bin",
    "90_1738776294.bin",
    "89_1738774871.bin",
    "54_1737471122.bin",
    "35_1737328492.bin"


    //"82_1737781669.bin",
    //"81_1737779607.bin",
    //"82_1737781669.bin",
    //"81_1737779607.bin",
    //"78_1737778725.bin",
    //"80_1737779377.bin",
    //"82_1737781669.bin",
    //"81_1737779607.bin",
    //"78_1737778725.bin",
    //"80_1737779377.bin"
};
void process_button_press(uint32_t press_type) {
    switch (press_type) {
        case BUTTON_SINGLE_PRESS:
            ESP_LOGI(TAG, "Single press detected");
            /* 
            //demo program:
            demo_current++;
            if (demo_current >= 10) {
                demo_current = 0;
            }
            display_image(demo_queue[demo_current]);
             */


            //normal mode:
            server_sync();


             /*
            disable_wifi();
            renderer_init();
            board_poweron(&ctrl_state);
            clear();
            //display_image(3);
            board_poweroff(&ctrl_state);
            enable_wifi();
            vTaskDelay(pdMS_TO_TICKS(2000));
            check_updates();
            vTaskDelay(pdMS_TO_TICKS(2000));
            disable_wifi();
            */

            //waveform mode:
            /*
            get_waveform();
            renderer_init();
            board_poweron(&ctrl_state);
            clear();
            display_image("vert_stripes.bin");
            board_poweroff(&ctrl_state);
            */
            break;
            
        case BUTTON_DOUBLE_PRESS:
            ESP_LOGI(TAG, "Double press detected");
           // vTaskDelay(pdMS_TO_TICKS(200));
            demo_current--;
            if (demo_current < 0) {
                demo_current = 9;
            }
            display_image(demo_queue[demo_current]);
            break;
            
        case BUTTON_PRESS_3S:
            ESP_LOGI(TAG, "3s press detected");
            break;
            
        case BUTTON_PRESS_6S:
            ESP_LOGI(TAG, "6s press detected");
            vTaskDelay(pdMS_TO_TICKS(100)); // Optional: small delay before reboot
            esp_restart(); // Reboot the ESP32
            break;
    }
}

void configure_rtc_gpio(int gpio_num) {
    rtc_gpio_deinit(gpio_num);
    rtc_gpio_init(gpio_num);
    rtc_gpio_set_direction(gpio_num, RTC_GPIO_MODE_INPUT_ONLY);
    rtc_gpio_pullup_en(gpio_num);
    rtc_gpio_pulldown_dis(gpio_num);
    rtc_gpio_hold_en(gpio_num);
}

void deconfigure_rtc_gpio(int gpio_num) {
    rtc_gpio_hold_dis(gpio_num);  // Disable hold function first
    rtc_gpio_deinit(gpio_num);
}

void button_init(void)
{
    esp_err_t ret;

    rtc_gpio_hold_dis(BTN);

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_NEGEDGE;
    io_conf.pin_bit_mask = 1ULL << BTN;
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pull_up_en = GPIO_PULLUP_ENABLE;
    ret = gpio_config(&io_conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error configuring GPIO: %s", esp_err_to_name(ret));
        return;
    }

    ret = gpio_install_isr_service(0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error installing GPIO ISR service: %s", esp_err_to_name(ret));
        return;
    }

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    if (gpio_evt_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create event queue");
        return;
    }

    TaskHandle_t task_handle;
    BaseType_t task_created = xTaskCreate(button_task, "button_task", BUTTON_TASK_STACK_SIZE, NULL, 10, &task_handle);
    if (task_created != pdPASS) {
        ESP_LOGE(TAG, "Failed to create button task");
        return;
    }

    ret = gpio_isr_handler_add(BTN, gpio_isr_handler, (void*) BTN);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error adding ISR handler: %s", esp_err_to_name(ret));
        return;
    }

    ESP_LOGI(TAG, "Button initialized with RTC GPIO support");
}
