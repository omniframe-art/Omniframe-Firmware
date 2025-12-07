#include <stdio.h>
#include <string.h>
#include <sys/stat.h>   // Include for directory creation
#include <dirent.h>     // Include for directory operations
#include "esp_system.h"
#include "esp_log.h"
#include "driver/gpio.h"
#include "driver/sdmmc_host.h"
#include "driver/sdmmc_defs.h"
#include "ff.h" // FATFS library
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "sd_card.h"
#include "config.h"
#include <unistd.h>
#include <errno.h>      // Required for errno and strerror

#define TAG "SD_CARD"

sdmmc_card_t* card;
static bool sd_is_mounted = false;

void sd_powerdown(){
    ESP_LOGI(TAG, "Powering down SD card");
    // Set all SDIO lines to inputs with pull-downs to prevent floating or back-powering
    // gpio_set_pull_mode(SD_CMD, GPIO_PULLDOWN_ONLY);
    // gpio_set_pull_mode(SD_CLK, GPIO_PULLDOWN_ONLY);
    // gpio_set_pull_mode(SD_D0, GPIO_PULLDOWN_ONLY);
    // gpio_set_pull_mode(SD_D1, GPIO_PULLDOWN_ONLY);
    // gpio_set_pull_mode(SD_D2, GPIO_PULLDOWN_ONLY);
    // gpio_set_pull_mode(SD_D3, GPIO_PULLDOWN_ONLY);

    gpio_set_direction(SD_CMD, GPIO_MODE_INPUT);
    gpio_set_direction(SD_CLK, GPIO_MODE_INPUT);
    gpio_set_direction(SD_D0, GPIO_MODE_INPUT);
    gpio_set_direction(SD_D1, GPIO_MODE_INPUT);
    gpio_set_direction(SD_D2, GPIO_MODE_INPUT);
    gpio_set_direction(SD_D3, GPIO_MODE_INPUT);

    // Turn off SD card power
    gpio_set_level(SD_EN, 1);
}

void sd_powerup(void) {
    ESP_LOGI(TAG, "Powering up SD card");
    
    // Turn on SD card power
    gpio_set_level(SD_EN, 0);

    // Additional stabilization delay
    vTaskDelay(pdMS_TO_TICKS(100));
}

// Function to initialize the SD card
esp_err_t sd_init(void) {

    //sd_powerup(); //on v3 not needed

    gpio_reset_pin(SD_CLK);
    gpio_reset_pin(SD_CMD);
    gpio_reset_pin(SD_D0);
    gpio_reset_pin(SD_D1);
    gpio_reset_pin(SD_D2);
    gpio_reset_pin(SD_D3);

    vTaskDelay(pdMS_TO_TICKS(10));

    // Configure SDMMC slot with custom GPIOs
    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk = SD_CLK;
    slot_config.cmd = SD_CMD;
    slot_config.d0 = SD_D0;
    slot_config.d1 = SD_D1;
    slot_config.d2 = SD_D2;
    slot_config.d3 = SD_D3;
    slot_config.width = 4;

    // Mount configuration
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 10,
        .allocation_unit_size = 16 * 1024
    };

    // Initialize the SDMMC host
    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount filesystem. Error: %s", esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(1000));
        //sd_powerdown();
        return ret;
    }
    sd_is_mounted = true;
    // Card has been initialized, print some information about it
    sdmmc_card_print_info(stdout, card);
    ESP_LOGI(TAG, "SD card initialized successfully.");

    // const char* dir_path = "/sdcard/image_files";
    // if (mkdir(dir_path, 0775) != 0) {
    //     ESP_LOGW(TAG, "Failed to create directory directly. Error: %s", strerror(errno));
    // }

    // Define the directory path
    // DIR *dir = opendir(dir_path);
    // if (dir) {
    //     ESP_LOGI(TAG, "Listing files in %s:", dir_path);
    //     struct dirent *entry;
    //     while ((entry = readdir(dir)) != NULL) {
    //         ESP_LOGI(TAG, "Found file: %s", entry->d_name);
    //     }
    //     closedir(dir);
    // }
   
    //sd_unmount();
    return ESP_OK;
}

// Function to write data to a file on the SD card
esp_err_t sd_write_file(const char* path, const char* data) {
    ESP_LOGI(TAG, "Opening file %s", path);
    
   // esp_err_t ret = sd_mount();  // Mount first
   //  if (ret != ESP_OK) {
   //     return ret;
   // }

    FILE* f = fopen(path, "w");
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        sd_unmount();  // Unmount before returning
        return ESP_FAIL;
    }
    
    fprintf(f, "%s", data);
    fclose(f);
    ESP_LOGI(TAG, "File written successfully");
    return ESP_OK;
   // return sd_unmount();  // Unmount and return the unmount status
}


esp_err_t sd_read_file(const char* path, uint8_t* buffer, size_t len) {
   // esp_err_t ret = sd_mount();  // Mount first
   // if (ret != ESP_OK) {
   //     return ret;
   // }

    if (path == NULL || buffer == NULL || len == 0) {
        ESP_LOGE(TAG, "Invalid parameters");
        sd_unmount();  // Unmount before returning
        return ESP_ERR_INVALID_ARG;
    }

    ESP_LOGI(TAG, "Opening file %s", path);

    // Open file directly without redundant checks
    FILE *f = fopen(path, "rb");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open file: %s, errno: %d (%s)", path, errno, strerror(errno));
        //sd_unmount();  // Unmount before returning
        return ESP_FAIL;
    }

    // Get file size
    fseek(f, 0, SEEK_END);
    size_t file_size = ftell(f);
    fseek(f, 0, SEEK_SET);

    if (file_size > len) {
        ESP_LOGE(TAG, "Buffer too small. File size: %zu, Buffer size: %zu", file_size, len);
        fclose(f);
        //sd_unmount();  // Unmount before returning
        return ESP_ERR_INVALID_SIZE;
    }

    size_t read_len = fread(buffer, 1, file_size, f);
    if (read_len != file_size) {
        ESP_LOGE(TAG, "Read failed. Expected: %zu, Got: %zu, errno: %d (%s)", 
                 file_size, read_len, errno, strerror(errno));
        fclose(f);
        //sd_unmount();  // Unmount before returning
        return ESP_FAIL;
    }

    fclose(f);
    ESP_LOGI(TAG, "Successfully read %zu bytes from %s", read_len, path);
    //sd_unmount();  // Unmount before returning
    return ESP_OK;  // Unmount and return the unmount status
}

#define IMAGE_FILES_FOLDER "/sdcard/image_files"
#define MAX_PATH_LENGTH 256 // Adjust as needed for your filenames

bool exists_on_sd(const char* filename) {
   // esp_err_t ret = sd_mount();  // Mount first
   // if (ret != ESP_OK) {
   //     return false;
   // }

    char full_path[MAX_PATH_LENGTH];
    snprintf(full_path, sizeof(full_path), "%s/%s", IMAGE_FILES_FOLDER, filename);

    struct stat st;
    int result = stat(full_path, &st);
    bool exists = (result == 0);
    
    if (!exists) {
        ESP_LOGE(TAG, "File check failed for: %s, errno: %d (%s)", 
                 full_path, errno, strerror(errno));
    } else {
        ESP_LOGI(TAG, "File exists: %s", full_path);
    }

    //sd_unmount();  // Always unmount before returning
    return exists;
}

esp_err_t sd_mount(void) {
    if (sd_is_mounted) {
        ESP_LOGI(TAG, "SD card already mounted");
        return ESP_OK;
    }

    sd_powerup();

    sdmmc_host_t host = SDMMC_HOST_DEFAULT();
    // Adjust timing for more reliable communication
    host.max_freq_khz = SDMMC_FREQ_DEFAULT;  // Try lower frequency if issues persist
    host.command_timeout_ms = 1000;          // Increase timeout

    sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT();
    slot_config.clk = SD_CLK;
    slot_config.cmd = SD_CMD;
    slot_config.d0 = SD_D0;
    slot_config.d1 = SD_D1;
    slot_config.d2 = SD_D2;
    slot_config.d3 = SD_D3;
    slot_config.width = 4;
    slot_config.flags |= SDMMC_SLOT_FLAG_INTERNAL_PULLUP;  // Enable internal pullups

    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = false,
        .max_files = 10,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_card_t* card;
    esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &host, &slot_config, &mount_config, &card);
    
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card (error %s)", strerror(ret));
        sd_powerdown();
        return ret;
    }

    // Add card info logging
    ESP_LOGI(TAG, "SD Card mounted successfully:");
    ESP_LOGI(TAG, "Name: %s", card->cid.name);
    ESP_LOGI(TAG, "Capacity: %lluMB", ((uint64_t) card->csd.capacity) * card->csd.sector_size / (1024 * 1024));
    ESP_LOGI(TAG, "Speed: %dMHz", card->max_freq_khz / 1000);

    sd_is_mounted = true;
    return ESP_OK;
}

esp_err_t sd_unmount(void) {
    if (!sd_is_mounted) {
        ESP_LOGW(TAG, "SD card is not mounted");
        return ESP_OK;
    }

    // Flush pending writes
    // if (fflush(NULL) != 0) {
    //     ESP_LOGE(TAG, "Failed to flush filesystem buffers");
    //     return ESP_ERR_TIMEOUT;
    // }

    // Attempt to unmount
    ESP_LOGI(TAG, "Unmounting SD card...");
    esp_err_t ret = esp_vfs_fat_sdcard_unmount("/sdcard", card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to unmount SD card (error %d)", ret);
        return ret;
    }

    // Power down the SD card
    sd_powerdown();

    // De-init SD_EN GPIO
    gpio_reset_pin(SD_EN);
    
    sd_is_mounted = false;
    ESP_LOGI(TAG, "SD card unmounted successfully");
    return ESP_OK;
}

