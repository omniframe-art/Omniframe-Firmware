#ifndef DOWNLOAD_H
#define DOWNLOAD_H

#include "esp_err.h"

#define RX_BUFFER_SIZE 16384  // 16KB receive buffer
#define TX_BUFFER_SIZE 512

#define GALLERY_URL "https://omniframe-gallery.s3.amazonaws.com/"

// Function prototypes
esp_err_t download_image(const char* image_name, int slot);
esp_err_t start_image_download(const char *image_name, int slot);
esp_err_t get_name_nvs(int slot, char *image_name);
esp_err_t set_name_nvs(int slot, const char *image_name);

#endif // DOWNLOAD_H

