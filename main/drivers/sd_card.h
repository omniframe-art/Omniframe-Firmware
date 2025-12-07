#ifndef SD_CARD_H
#define SD_CARD_H

#include "esp_err.h"
#include <stdbool.h>

// Function prototypes
void sd_powerup();
void sd_powerdown();
esp_err_t sd_mount(void);
esp_err_t sd_unmount(void);
esp_err_t sd_init(void);
esp_err_t sd_write_file(const char* path, const char* data);
esp_err_t sd_read_file(const char* path, uint8_t* buffer, size_t len);
bool exists_on_sd(const char* filename);

#endif // SD_CARD_H
