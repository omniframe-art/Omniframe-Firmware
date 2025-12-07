#ifndef BUTTON_H
#define BUTTON_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "driver/gpio.h"
#include "esp_log.h"

// Button event types
typedef enum {
    BUTTON_SINGLE_PRESS,
    BUTTON_DOUBLE_PRESS,
    BUTTON_TRIPLE_PRESS,
    BUTTON_PRESS_3S,
    BUTTON_PRESS_6S,
    BUTTON_NO_ACTION
} button_event_t;

// Function prototypes
void configure_rtc_gpio(int gpio_num);
void deconfigure_rtc_gpio(int gpio_num);
void button_init(void);
void process_button_press(uint32_t press_count);

// If you want to make these constants configurable from outside the button module,
// you can declare them here as extern and define them in button.c
extern const int BUTTON_GPIO;
extern const int DEBOUNCE_TIME;
extern const int MAX_PRESS_INTERVAL;

#endif // BUTTON_H