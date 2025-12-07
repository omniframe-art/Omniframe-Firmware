#ifndef API_H
#define API_H

#include "esp_err.h"

// Base URL for API calls
#define BASE_URL "http://ec2-98-81-236-162.compute-1.amazonaws.com:5000"

// Buffer sizes
#define JSON_RESPONSE_BUFFER_SIZE 2048

// Waveform size
#define WAVEFORM_SIZE (SHADES * FRAMES)

// API function prototypes
esp_err_t server_sync(void);
esp_err_t send_device_info(void);
esp_err_t check_updates(void);
esp_err_t get_waveform(void);
esp_err_t hello(void);

#endif // API_H

