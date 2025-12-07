#include "image_queue.h"
#include "sd_card.h"
#include "image_data.h"
#include "download.h"
#include "nvs_flash.h"
#include "nvs.h"
#include <string.h> // For strncpy, memset

char current_queue[QUEUE_SIZE][MAX_FILENAME_LENGTH] = {0}; // Initialize with empty strings
char new_queue[QUEUE_SIZE][MAX_FILENAME_LENGTH] = {0};

/**
 * @brief Retrieves the image queue from NVS.
 */
esp_err_t get_queue_nvs(void) {
    ESP_LOGI("NVS", "Getting queue from NVS");

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    size_t queue_size = sizeof(current_queue);
    err = nvs_get_blob(nvs_handle, NVS_QUEUE_KEY, current_queue, &queue_size);
    if (err == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW("NVS", "Queue not found in NVS. Initializing to empty.");
        memset(current_queue, 0, sizeof(current_queue)); // Initialize to empty
        err = ESP_OK; // No error, just not found
    } else if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to get queue from NVS: %s", esp_err_to_name(err));
    } else {
        ESP_LOGI("NVS", "Queue retrieved successfully from NVS.");
        // Print the retrieved queue
        ESP_LOGI("NVS", "Current queue contents:");
        for (int i = 0; i < QUEUE_SIZE; i++) {
            if (current_queue[i][0] != '\0') {
                ESP_LOGI("NVS", "Position %d: %s", i, current_queue[i]);
            } else {
                ESP_LOGI("NVS", "Position %d: <empty>", i);
            }
        }
    }

    nvs_close(nvs_handle);
    return err;
}

/**
 * @brief Stores the image queue to NVS.
 */
esp_err_t set_queue_nvs(void) {
    ESP_LOGI("NVS", "Setting queue to NVS");

    nvs_handle_t nvs_handle;
    esp_err_t err = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to open NVS: %s", esp_err_to_name(err));
        return err;
    }

    err = nvs_set_blob(nvs_handle, NVS_QUEUE_KEY, current_queue, sizeof(current_queue));
    if (err != ESP_OK) {
        ESP_LOGE("NVS", "Failed to set queue to NVS: %s", esp_err_to_name(err));
    } else {
        err = nvs_commit(nvs_handle);
        if (err != ESP_OK) {
            ESP_LOGE("NVS", "Failed to commit queue to NVS: %s", esp_err_to_name(err));
        } else {
            ESP_LOGI("NVS", "Queue stored successfully to NVS.");
        }
    }

    nvs_close(nvs_handle);
    return err;
}

/**
 * @brief Compares the current queue with a new queue, handles differences, and updates the display.
 */
void compare_queues(void) {
    extern char new_queue[QUEUE_SIZE][MAX_FILENAME_LENGTH]; 
    esp_err_t err = ESP_OK;

    for (int i = 0; i < QUEUE_SIZE; i++) {
        ESP_LOGI("Comparing", "cur: %s, new: %s", current_queue[i], new_queue[i]);
        if (strcmp(current_queue[i], new_queue[i]) != 0) {
            if (!exists_on_sd(new_queue[i])) {    
                err = start_image_download(new_queue[i], i); 
                if (err != ESP_OK) {
                    ESP_LOGE("Image Download Failed", "Error: %s", esp_err_to_name(err));
                }
            }
            if (i == 0 && err == ESP_OK) {
                display_image(new_queue[i]);
            }
        }
        strncpy(current_queue[i], new_queue[i], MAX_FILENAME_LENGTH);
        if (err == ESP_OK) {
            set_queue_nvs(); // Save updated queue to NVS
        }
    }
    
}

/**
 * @brief Advances the queue by rotating it forward.
 */
void advance_queue(void) {
    char temp[MAX_FILENAME_LENGTH];
    strncpy(temp, current_queue[0], MAX_FILENAME_LENGTH);
    for (int i = 0; i < QUEUE_SIZE - 1; i++) {
        strncpy(current_queue[i], current_queue[i + 1], MAX_FILENAME_LENGTH);
    }
    strncpy(current_queue[QUEUE_SIZE - 1], temp, MAX_FILENAME_LENGTH);
    set_queue_nvs();
}

/**
 * @brief Moves the queue backward by rotating it in reverse.
 */
void move_back_queue(void) {
    char temp[MAX_FILENAME_LENGTH];
    strncpy(temp, current_queue[QUEUE_SIZE - 1], MAX_FILENAME_LENGTH);
    for (int i = QUEUE_SIZE - 1; i > 0; i--) {
        strncpy(current_queue[i], current_queue[i - 1], MAX_FILENAME_LENGTH);
    }
    strncpy(current_queue[0], temp, MAX_FILENAME_LENGTH);
    set_queue_nvs();
}
