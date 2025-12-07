#ifndef WIFI_H
#define WIFI_H

#include "esp_wifi.h"
#include "esp_event.h"

// WiFi connection parameters
#define DEFAULT_WIFI_SSID      "Verizon_B9QR34"
#define DEFAULT_WIFI_PASS      "tout7-fen-crust"

#define EXAMPLE_ESP_MAXIMUM_RETRY  5

// Event group bits
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

// Function declarations
void nvs_init(void);
void setup_wifi(void);
void disable_wifi(void);
void enable_wifi(void);

#endif // WIFI_H
