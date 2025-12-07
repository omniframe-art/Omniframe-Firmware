#include "lut.h"

#include "render_method.h"
#include "render_context.h"
#include <stdio.h>  // Make sure to include this for printf
#include <stdint.h>

#include <string.h>

#include "esp_log.h"
#include "esp_partition.h"
#include "esp_err.h"

#define IMAGES_PARTITION_LABEL "images"
#define WAVEFORM_OFFSET 0x45E000

static const char *TAG = "waveform_flash";

uint8_t custom_wave[16][30] = {
  {2, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0}, //0
  {2, 0, 1, 1, 1, 1, 1, 1, 1, 0, 2, 0, 1, 1, 1, 1, 1, 0}, //1
  {2, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 0, 2, 0, 1, 1, 1, 0}, //2
  {2, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 1, 1, 0, 2, 1, 0}, //3
    
  {2, 0, 1, 1, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0, 2, 0}, //4
  {2, 2, 2, 2, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0, 2, 0}, //5
  {2, 2, 2, 2, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 2, 0}, //6
  {2, 2, 2, 2, 0, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 2, 0}, //7

  {2, 2, 2, 2, 0, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 2, 0}, //8
  {2, 2, 2, 2, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 2, 0, 2, 0}, //9
  {2, 0, 2, 2, 2, 0, 1, 1, 1, 1, 1, 1, 1, 1, 0, 2, 2, 0}, //10
  {2, 2, 2, 2, 0, 1, 1, 1, 0, 1, 1, 1, 1, 1, 0, 2, 2, 0}, //11

  {2, 2, 2, 2, 2, 0, 1, 1, 1, 1, 1, 1, 0, 2, 2, 0, 2, 0}, //12
  {2, 0, 2, 0, 2, 2, 2, 0, 1, 1, 1, 0, 0, 2, 2, 0, 2, 0}, //13
  {2, 0, 2, 2, 2, 2, 2, 2, 0, 1, 1, 0, 2, 2, 0, 2, 2, 0}, //14
  {2, 0, 2, 2, 2, 2, 2, 2, 2, 2, 0, 2, 2, 2, 2, 2, 2, 0}, //15
};

void save_waveform() {
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, IMAGES_PARTITION_LABEL);

    if (partition == NULL) {
        ESP_LOGE(TAG, "Failed to find the images partition!");
        return;
    }

    // Erase the sector before writing
    esp_err_t err = esp_partition_erase_range(partition, WAVEFORM_OFFSET, 4096);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to erase flash region: %s", esp_err_to_name(err));
        return;
    }

    // Write the waveform data to flash
    err = esp_partition_write(partition, WAVEFORM_OFFSET, custom_wave, WAVEFORM_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write waveform data to flash: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Waveform data written successfully to flash!");
    }
}

void load_waveform() {
    const esp_partition_t *partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA, ESP_PARTITION_SUBTYPE_ANY, IMAGES_PARTITION_LABEL);

    if (partition == NULL) {
        ESP_LOGE(TAG, "Failed to find the images partition!");
        return;
    }

    esp_err_t err = esp_partition_read(partition, WAVEFORM_OFFSET, custom_wave, WAVEFORM_SIZE);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read waveform data from flash: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI(TAG, "Waveform data read successfully from flash!");
    }
}

__attribute__((optimize("O3")))
void IRAM_ATTR custom_lut_func(
    const uint32_t *ld,
    uint8_t *epd_input,
    uint8_t* lut,
    uint8_t frame
) {
    uint16_t *ptr = (uint16_t *)ld;
    //uint8_t *ops = (uint8_t *)frame_ops;
    uint16_t temp;

    // Local variables to store wave lookup values
    uint8_t wave0, wave1, wave2, wave3;

    for (uint32_t j = 0; j < 400; j += 1) {
       // epd_input[j] = lut[*(ptr++)];
        temp = *(ptr++);

        wave0 = custom_wave[temp & 0x000F][frame];
        wave1 = custom_wave[(temp & 0x00F0) >> 4][frame];
        wave2 = custom_wave[(temp & 0x0F00) >> 8][frame];
        wave3 = custom_wave[(temp & 0xF000) >> 12][frame];

        epd_input[j] = (wave3 << 6) | (wave2 << 4) | (wave1 << 2) | wave0;
        
    }
}

__attribute__((optimize("O3")))
void IRAM_ATTR calculate_lut(RenderContext_t *ctx) {
    uint8_t frame = ctx->current_frame;
    uint16_t index = 0;
    uint8_t first_op, second_op, third_op, fourth_op;
    uint8_t comb_op;

    for (uint8_t first = 0; first < 16; first++) {
        first_op = custom_wave[first][frame] << 6;

        for (uint8_t second = 0; second < 16; second++) {
            second_op = custom_wave[second][frame] << 4;

            for (uint8_t third = 0; third < 16; third++) {
                third_op = custom_wave[third][frame] << 2;

                for (uint8_t fourth = 0; fourth < 16; fourth++) {
                    fourth_op = custom_wave[fourth][frame];

                    comb_op = first_op | second_op | third_op | fourth_op;
                    ctx->conversion_lut[index++] = comb_op;
                }
            }
        }
    }
}






