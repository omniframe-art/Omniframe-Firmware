#include "image_data.h"
#include "config.h"
#include "esp_log.h"
#include "esp_partition.h"
#include "sd_card.h"
#include "download.h"
#include "wifi.h"
#include <string.h>


#include "../../components/epdiy/src/epd_highlevel.h"
#include "../../components/epdiy/src/render.h"
#include "../../components/epdiy/src/epdiy.h"


EpdRect full_area = {.x = 0, .y = 0, .width = 1600, .height = 1200};

void clear(){
    ESP_LOGI("Clearing", "...");
    epd_clear_area_cycles(full_area, 2);
}

static uint8_t current_idx = 3;

#define MAX_FILENAME_LENGTH 128
#define IMAGE_DATA_SIZE 960000  // 960 KB
#define ENTRY_SIZE (MAX_FILENAME_LENGTH + IMAGE_DATA_SIZE)

typedef struct {
    char filename[MAX_FILENAME_LENGTH];
    uint8_t data[IMAGE_DATA_SIZE];
} ImageEntry;

static const char* TAG = "ImageFlash";


bool write_to_flash(const char* filename, const uint8_t* data, uint32_t index) {
    const esp_partition_t* partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        ESP_PARTITION_SUBTYPE_ANY,
        "images"
    );

    if (partition == NULL) {
        ESP_LOGE(TAG, "Failed to find partition");
        return false;
    }

    ImageEntry entry;
    memset(&entry, 0, sizeof(ImageEntry));
    strncpy(entry.filename, filename, MAX_FILENAME_LENGTH - 1);
    memcpy(entry.data, data, IMAGE_DATA_SIZE);

    uint32_t offset = index * ENTRY_SIZE;
    esp_err_t err = esp_partition_write(partition, offset, &entry, sizeof(ImageEntry));
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to write to partition: %s", esp_err_to_name(err));
        return false;
    }

   // ESP_LOGI(TAG, "Successfully wrote '%s' to flash at index %d", filename, index);
    return true;
}

bool get_from_sd(uint8_t* framebuffer, size_t framebuffer_size, char* filename) {
    // Allocate buffer large enough for the path
    char path[256];  // Make sure this is big enough for your longest possible path
    // Use snprintf for safe string concatenation
    snprintf(path, sizeof(path), "/sdcard/image_files/%s", filename);
    
    ESP_LOGI(TAG, "Reading from SD card: %s", path);

    return sd_read_file(path, framebuffer, framebuffer_size) == ESP_OK;
}

bool get_from_flash(uint8_t* framebuffer, size_t framebuffer_size, uint32_t index) {
    const esp_partition_t* partition = esp_partition_find_first(
        ESP_PARTITION_TYPE_DATA,
        IMAGE_PARTITION_SUBTYPE,
        "images"
    );

    if (partition == NULL) {
        ESP_LOGE("get_from_flash", "Failed to find partition");
        return false;
    }

    // Calculate the offset based on the index
    // Each image is 1MB (1048576 bytes) apart
    uint32_t offset = index * 960*1024;

    // Check if the requested image is within the partition bounds
    if (offset + framebuffer_size > partition->size) {
        ESP_LOGE("get_from_flash", "Image index out of bounds");
        return false;
    }

    esp_err_t err = esp_partition_read(partition, offset, framebuffer, framebuffer_size);
    if (err != ESP_OK) {
        ESP_LOGE("get_from_flash", "Failed to read partition: %s", esp_err_to_name(err));
        return false;
    }

   // ESP_LOGI("get_from_flash", "Successfully read image %d from flash", int(index));
    return true;
}

void display_image(char* filename) {
    //disable_wifi();
    renderer_init();
    board_poweron(&ctrl_state);
    clear();
    ESP_LOGI("Displaying image", "%s", filename);
    size_t framebuffer_size = 1600 / 2 * 1200;
    uint8_t* framebuffer = epd_hl_get_framebuffer(&hl_state);

   // if (!get_from_flash(framebuffer, framebuffer_size, index)) {
   //     ESP_LOGE("display_image", "Failed to get data from flash");
   //     return;
    //}

   // char filename[MAX_FILENAME_LENGTH];
   // get_name_nvs(index, filename);
    
    if (!get_from_sd(framebuffer, framebuffer_size, filename)) {
        ESP_LOGE("display_image", "Failed to get data from SD");
        return;
    }

    enum EpdDrawError _err = epd_hl_update_screen(&hl_state, MODE_GC16, 25);

    if (_err != EPD_DRAW_SUCCESS) {
        ESP_LOGE("display_image", "Failed to update screen: %d", _err);
    }
    board_poweroff(&ctrl_state);
    //enable_wifi();
}

void display_next_image(){
    if(current_idx == 3){
        current_idx = 0;
    }
    else{
        current_idx++; 
    }
    clear();
    //display_image(current_idx);
}