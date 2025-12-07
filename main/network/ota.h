#ifndef OTA_H
#define OTA_H

#include <stddef.h>
#include "esp_err.h"
#include <inttypes.h>

extern char current_firmware_version[32];  // Declare it as external

void check_ota_updates(void);
void ota_init(void);

#endif // OTA_H
