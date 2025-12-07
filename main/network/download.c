#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_partition.h"
#include "esp_timer.h"
#include "esp_crt_bundle.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "download.h"
#include "config.h"

static const char *TAG = "DOWNLOAD";

#define SLOT_SIZE 960*1024
#define NVS_NAMESPACE "storage"
#define MAX_URL_LENGTH (sizeof(GALLERY_URL) + MAX_FILENAME_LENGTH)
#define SD_MOUNT_POINT "/sdcard/image_files/"
#define IMAGE_DOWNLOAD_TASK_STACK_SIZE 10240
#define IMAGE_DOWNLOAD_TASK_PRIORITY 5

static const esp_partition_t *images_partition = NULL;
static size_t write_offset = 0;
static int current_slot = 0;
static FILE *image_file = NULL;
static SemaphoreHandle_t download_semaphore;

// SD card image handler
static esp_err_t image_http_event_handler_sd(esp_http_client_event_t *evt) {
    switch (evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            return ESP_FAIL;

        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;

        case HTTP_EVENT_ON_DATA: {
            const char *filename = (const char *)evt->user_data;
            if (filename == NULL) {
                ESP_LOGE(TAG, "Filename not provided in user_data");
                return ESP_FAIL;
            }

            if (image_file == NULL) {
                char full_path[256];
                snprintf(full_path, sizeof(full_path), "%s%s", SD_MOUNT_POINT, filename);

                image_file = fopen(full_path, "wb");
                if (image_file == NULL) {
                    ESP_LOGE(TAG, "Failed to open file: %s", full_path);
                    return ESP_FAIL;
                }
                ESP_LOGI(TAG, "File opened for writing: %s", full_path);
            }

            if (fwrite(evt->data, 1, evt->data_len, image_file) != evt->data_len) {
                ESP_LOGE(TAG, "Failed to write data to file");
                fclose(image_file);
                image_file = NULL;
                return ESP_FAIL;
            }
            break;
        }

        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_FINISH");
            if (image_file) {
                fclose(image_file);
                image_file = NULL;
                ESP_LOGI(TAG, "File closed successfully");
            }
            break;

        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            if (image_file) {
                fclose(image_file);
                image_file = NULL;
            }
            break;

        default:
            break;
    }
    return ESP_OK;
}

// Flash partition image handler
static esp_err_t image_http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            return ESP_FAIL;

        case HTTP_EVENT_ON_CONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_ON_CONNECTED");
            break;

        case HTTP_EVENT_ON_DATA:
            if (images_partition == NULL) {
                images_partition = esp_partition_find_first(ESP_PARTITION_TYPE_DATA, 0x82, "images");
                if (images_partition == NULL) {
                    ESP_LOGE(TAG, "Failed to find images partition");
                    return ESP_FAIL;
                }
                esp_err_t err = esp_partition_erase_range(images_partition, current_slot * SLOT_SIZE, SLOT_SIZE);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG, "Failed to erase partition slot %d: %s", current_slot, esp_err_to_name(err));
                    return err;
                }
                vTaskDelay(1);
            }

            esp_err_t err = esp_partition_write(images_partition, current_slot * SLOT_SIZE + write_offset, evt->data, evt->data_len);
            if (err != ESP_OK) {
                ESP_LOGE(TAG, "Failed to write to flash slot %d: %s", current_slot, esp_err_to_name(err));
                return err;
            }
            write_offset += evt->data_len;
            break;

        case HTTP_EVENT_ON_FINISH:
            ESP_LOGI(TAG, "Image download complete for slot %d, size=%d bytes", current_slot, write_offset);
            write_offset = 0;
            break;

        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG, "HTTP_EVENT_DISCONNECTED");
            write_offset = 0;
            images_partition = NULL;
            break;

        default:
            break;
    }
    return ESP_OK;
}

esp_err_t get_name_nvs(int slot, char *image_name) {
    char key_name[16];
    snprintf(key_name, sizeof(key_name), "image_slot_%d", slot);

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    size_t max_len = MAX_FILENAME_LENGTH;
    ret = nvs_get_str(nvs_handle, key_name, image_name, &max_len);
    if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Key '%s' not found in NVS", key_name);
    } else if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to read string from NVS: %s", esp_err_to_name(ret));
    }

    nvs_close(nvs_handle);
    return ret;
}

esp_err_t set_name_nvs(int slot, const char *image_name) {
    char key_name[16];
    snprintf(key_name, sizeof(key_name), "image_slot_%d", slot);

    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_str(nvs_handle, key_name, image_name);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set string in NVS: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit changes in NVS: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    nvs_close(nvs_handle);
    ESP_LOGI(TAG, "Successfully saved '%s' under key '%s' in NVS", image_name, key_name);
    return ESP_OK;
}

esp_err_t download_image(const char* image_name, int slot) {
    char image_url[MAX_URL_LENGTH];
    snprintf(image_url, MAX_URL_LENGTH, "%s%s", GALLERY_URL, image_name);

    ESP_LOGI(TAG, "Downloading image from %s into slot %d", image_url, slot);

    int64_t start_time = esp_timer_get_time();

    esp_http_client_config_t config = {
        .url = image_url,
        .event_handler = image_http_event_handler_sd,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = RX_BUFFER_SIZE,
        .buffer_size_tx = TX_BUFFER_SIZE,
        .user_data = (void *)image_name,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    write_offset = 0;
    images_partition = NULL;
    current_slot = slot;

    esp_err_t err = esp_http_client_perform(client);

    int64_t end_time = esp_timer_get_time();
    float download_time = (end_time - start_time) / 1000000.0;

    if (err == ESP_OK) {
        float download_speed = 960000 / (1024.0 * download_time);
        ESP_LOGI(TAG, "Image download succeeded for slot %d", slot);
        ESP_LOGI(TAG, "Download stats: Time: %.2f seconds, Speed: %.2f KB/s", download_time, download_speed);
        set_name_nvs(slot, image_name);
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed for slot %d: %s", slot, esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}

typedef struct {
    char *image_name;
    int slot;
    esp_err_t result;
} download_params_t;

static void image_download_task(void *pvParameters) {
    download_params_t *params = (download_params_t *)pvParameters;

    params->result = download_image(params->image_name, params->slot);

    if (params->result == ESP_OK) {
        ESP_LOGI(TAG, "Image download succeeded for slot %d", params->slot);
    } else {
        ESP_LOGE(TAG, "Image download failed for slot %d", params->slot);
    }

    free(params->image_name);
    xSemaphoreGive(download_semaphore);
    vTaskDelete(NULL);
}

esp_err_t start_image_download(const char *image_name, int slot) {
    download_params_t *params = malloc(sizeof(download_params_t));
    if (params == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for download parameters");
        return ESP_ERR_NO_MEM;
    }

    params->image_name = strdup(image_name);
    if (params->image_name == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for image URL");
        free(params);
        return ESP_ERR_NO_MEM;
    }
    params->slot = slot;

    download_semaphore = xSemaphoreCreateBinary();

    if (xTaskCreate(&image_download_task, "image_download_task",
                    IMAGE_DOWNLOAD_TASK_STACK_SIZE,
                    (void *)params,
                    IMAGE_DOWNLOAD_TASK_PRIORITY,
                    NULL) != pdPASS) {
        ESP_LOGE(TAG, "Failed to create image download task");
        free(params->image_name);
        free(params);
        vSemaphoreDelete(download_semaphore);
        return ESP_FAIL;
    }

    xSemaphoreTake(download_semaphore, portMAX_DELAY);

    esp_err_t result = params->result;

    free(params);
    vSemaphoreDelete(download_semaphore);

    return result;
}

