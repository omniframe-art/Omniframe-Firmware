#include "ota.h"
#include "wifi.h"
#include "api.h"
#include "esp_ota_ops.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "esp_system.h"
#include "nvs_flash.h"
#include "nvs.h"
#include "cJSON.h"
#include "string.h"
#include "ctype.h"
#include "esp_crt_bundle.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#define DEFAULT_FIRMWARE_VERSION "1.0.0"
#define NVS_NAMESPACE "storage"

#define JSON_RESPONSE_BUFFER_SIZE 1024
#define RX_BUFFER_SIZE 16384  // 16KB receive buffer
#define TX_BUFFER_SIZE 512

static const char *TAG = "OTA";
static const char *TAG2 = "HTTP_CHECK";
static const char *TAG3 = "HTTP_OTA";

char current_firmware_version[32];
static char json_response_buffer[JSON_RESPONSE_BUFFER_SIZE];  // JSON response buffer
static int json_buffer_index = 0;  // Index for JSON buffer

static esp_ota_handle_t ota_handle = 0;

typedef struct {
    char update_url[256];
    char new_version[32];
} ota_update_params_t;

static SemaphoreHandle_t ota_semaphore = NULL;

esp_err_t check_update_http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            // ESP_LOGI(TAG2, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_CONNECTED:
            // ESP_LOGI(TAG2, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADER_SENT:
            // ESP_LOGI(TAG2, "HTTP_EVENT_HEADER_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            //ESP_LOGI(TAG2, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            if (evt->data_len + json_buffer_index < sizeof(json_response_buffer)) {
                memcpy(json_response_buffer + json_buffer_index, evt->data, evt->data_len);
                json_buffer_index += evt->data_len;
            } else {
                ESP_LOGE(TAG2, "Response buffer overflow");
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            // ESP_LOGI(TAG2, "HTTP_EVENT_ON_FINISH");
            json_response_buffer[json_buffer_index] = '\0';  // Null-terminate the response
           // ESP_LOGI(TAG2, "Received response: %s", json_response_buffer);
            break;
        case HTTP_EVENT_DISCONNECTED:
         //   ESP_LOGI(TAG2, "HTTP_EVENT_DISCONNECTED");
            break;
        default:
            ESP_LOGW(TAG2, "Unhandled event id: %d", evt->event_id);
            break;
    }
    return ESP_OK;
}

esp_err_t check_for_updates(char *firmware_url, size_t url_len, char *new_version, size_t version_len) {
    char get_url[256];
    snprintf(get_url, sizeof(get_url), "%s/check-updates/%ld", BASE_URL, SERIAL_NUMBER);

    esp_http_client_config_t config = {
        .url = get_url,
        .method = HTTP_METHOD_GET,
        .event_handler = check_update_http_event_handler,
    };

    esp_http_client_handle_t client = esp_http_client_init(&config);

    //ESP_LOGI(TAG, "Making GET request to %s", get_url);

    json_buffer_index = 0;  // Reset JSON buffer index

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
      //  ESP_LOGI(TAG, "HTTP GET Status = %d", esp_http_client_get_status_code(client));
    } else {
        ESP_LOGE(TAG, "HTTP GET request failed: %s", esp_err_to_name(err));
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    // Ensure the JSON response buffer is null-terminated
    if (json_buffer_index < JSON_RESPONSE_BUFFER_SIZE) {
        json_response_buffer[json_buffer_index] = '\0';
    } else {
        json_response_buffer[JSON_RESPONSE_BUFFER_SIZE - 1] = '\0';
    }

    // Parse JSON response
    cJSON *json = cJSON_Parse(json_response_buffer);
    if (json == NULL) {
        ESP_LOGE(TAG, "Failed to parse JSON response: %s", cJSON_GetErrorPtr());
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    cJSON *version_item = cJSON_GetObjectItem(json, "version");
    cJSON *url_item = cJSON_GetObjectItem(json, "url");
    if (cJSON_IsString(version_item) && cJSON_IsString(url_item)) {
        snprintf(firmware_url, url_len, "%s", url_item->valuestring);
        snprintf(new_version, version_len, "%s", version_item->valuestring);

       // ESP_LOGI(TAG, "Newest firmware URL: %s", firmware_url);
         //ESP_LOGI(TAG, "Newest firmware version: %s", new_version);

        if (strcmp(current_firmware_version, new_version) != 0) {
            cJSON_Delete(json);
            esp_http_client_cleanup(client);
            return ESP_OK; // New version available, proceed with update
        } else {
            ESP_LOGI(TAG, "OTA checked, up to date: %s", current_firmware_version);
        }
    } else {
        ESP_LOGE(TAG, "Invalid JSON response format");
        cJSON_Delete(json);
        esp_http_client_cleanup(client);
        return ESP_FAIL;
    }

    cJSON_Delete(json);
    esp_http_client_cleanup(client);
    return ESP_FAIL; // No new version available
}


esp_err_t ota_http_event_handler(esp_http_client_event_t *evt) {
    static const esp_partition_t *ota_partition = NULL;
    static size_t write_offset = 0;
    static esp_ota_handle_t ota_handle = 0;  // Ensure ota_handle is declared

    esp_err_t err;  // Declare err at the beginning of the function

    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            //ESP_LOGE(TAG3, "HTTP_EVENT_ERROR");
            return ESP_FAIL;
        case HTTP_EVENT_ON_CONNECTED:
            //ESP_LOGI(TAG3, "HTTP_EVENT_ON_CONNECTED");
            break;
        case HTTP_EVENT_HEADERS_SENT:
            //ESP_LOGI(TAG3, "HTTP_EVENT_HEADERS_SENT");
            break;
        case HTTP_EVENT_ON_HEADER:
            //ESP_LOGI(TAG3, "HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
            break;
        case HTTP_EVENT_ON_DATA:
            if (ota_partition == NULL) {
                ota_partition = esp_ota_get_next_update_partition(NULL);
                if (ota_partition == NULL) {
                    ESP_LOGE(TAG3, "Failed to find OTA partition");
                    return ESP_FAIL;
                }
                err = esp_ota_begin(ota_partition, OTA_SIZE_UNKNOWN, &ota_handle);
                if (err != ESP_OK) {
                    ESP_LOGE(TAG3, "Failed to begin OTA: %s", esp_err_to_name(err));
                    return err;
                }
            }

            err = esp_ota_write(ota_handle, evt->data, evt->data_len);
            if (err != ESP_OK) {
                ESP_LOGE(TAG3, "Failed to write to OTA partition: %s", esp_err_to_name(err));
                return err;
            }
            write_offset += evt->data_len;
            break;
        case HTTP_EVENT_ON_FINISH:
            // ESP_LOGI(TAG3, "HTTP_EVENT_ON_FINISH");
            ESP_LOGI(TAG3, "OTA download complete, size=%d bytes", write_offset);
            write_offset = 0;
            if (esp_ota_end(ota_handle) != ESP_OK) {
                ESP_LOGE(TAG3, "esp_ota_end failed!");
                return ESP_FAIL;
            }
            err = esp_ota_set_boot_partition(ota_partition);
            if (err != ESP_OK) {
                ESP_LOGE(TAG3, "esp_ota_set_boot_partition failed! err=0x%x", err);
                return err;
            }
            ESP_LOGI(TAG3, "esp_ota_set_boot_partition succeeded");
            break;
        case HTTP_EVENT_DISCONNECTED:
            ESP_LOGI(TAG3, "HTTP_EVENT_DISCONNECTED");
            write_offset = 0;
            ota_partition = NULL;
            break;
        default:
            ESP_LOGW(TAG3, "Unhandled HTTP event: %d", evt->event_id);
            break;
    }
    return ESP_OK;
}

esp_err_t perform_ota_update(const char *update_url) {
    esp_http_client_config_t config = {
        .url = update_url,
        .event_handler = ota_http_event_handler,
        .crt_bundle_attach = esp_crt_bundle_attach,
        .buffer_size = RX_BUFFER_SIZE,     // Set the receive buffer size
        .buffer_size_tx = TX_BUFFER_SIZE   // Set a smaller transmit buffer size
    };

    esp_err_t ret;
    const esp_partition_t *update_partition = esp_ota_get_next_update_partition(NULL);
    if (update_partition == NULL) {
        ESP_LOGE(TAG, "Unable to find update partition");
        return ESP_FAIL;
    }

    esp_http_client_handle_t client = esp_http_client_init(&config);
    if (client == NULL) {
        ESP_LOGE(TAG, "Failed to initialise HTTP connection");
        return ESP_FAIL;
    }

    ret = esp_http_client_open(client, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open HTTP connection: %s", esp_err_to_name(ret));
        esp_http_client_cleanup(client);
        return ret;
    }

    esp_http_client_fetch_headers(client);
    ret = esp_ota_begin(update_partition, OTA_SIZE_UNKNOWN, &ota_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed, error=%d", ret);
        esp_http_client_cleanup(client);
        return ret;
    }

    ESP_LOGI(TAG, "esp_ota_begin succeeded");

    char ota_write_data[1024];
    int binary_file_length = 0;
    while (1) {
        int data_read = esp_http_client_read(client, ota_write_data, sizeof(ota_write_data));
        if (data_read < 0) {
            ESP_LOGE(TAG, "Error: SSL data read error");
            ret = ESP_FAIL;
            break;
        } else if (data_read == 0) {
            ESP_LOGI(TAG, "Connection closed, all data received");
            break;
        } else {
            ret = esp_ota_write(ota_handle, (const void *)ota_write_data, data_read);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "esp_ota_write failed! err=0x%x", ret);
                break;
            }
            binary_file_length += data_read;
            //ESP_LOGI(TAG, "Written image length %d", binary_file_length);
        }
    }

    if (esp_ota_end(ota_handle) != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_end failed!");
        ret = ESP_FAIL;
    } else {
        ret = esp_ota_set_boot_partition(update_partition);
        if (ret != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed! err=0x%x", ret);
        } else {
            ESP_LOGI(TAG, "esp_ota_set_boot_partition succeeded");
        }
    }

    esp_http_client_cleanup(client);
    return ret;
}

#define OTA_UPDATE_TASK_STACK_SIZE 8192  // Adjust stack size as needed
#define OTA_UPDATE_TASK_PRIORITY 5  
 #define FW_VERSION_KEY "fw_version"  // Shortened key name

esp_err_t save_firmware_version(const char* version) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_str(nvs_handle, FW_VERSION_KEY, version);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to set firmware version in NVS: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to commit NVS: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Successfully saved firmware version: %s", version);
    }

    nvs_close(nvs_handle);
    return ret;
}

esp_err_t load_firmware_version(char* version, size_t max_len) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to open NVS handle: %s", esp_err_to_name(ret));
        return ret;
    }

    size_t required_size = 0;
    ret = nvs_get_str(nvs_handle, FW_VERSION_KEY, NULL, &required_size);
    if (ret != ESP_OK) {
        if (ret == ESP_ERR_NVS_NOT_FOUND) {
            ESP_LOGW(TAG, "Firmware version not found in NVS");
        } else {
            ESP_LOGE(TAG, "Error reading firmware version size: %s", esp_err_to_name(ret));
        }
        nvs_close(nvs_handle);
        return ret;
    }

    if (required_size > max_len) {
        ESP_LOGE(TAG, "Firmware version string too long");
        nvs_close(nvs_handle);
        return ESP_ERR_NVS_VALUE_TOO_LONG;
    }

    ret = nvs_get_str(nvs_handle, FW_VERSION_KEY, version, &required_size);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Error reading firmware version: %s", esp_err_to_name(ret));
    } 
    nvs_close(nvs_handle);
    return ret;
}

void ota_update_task(void *pvParameters) {
    ota_update_params_t *params = (ota_update_params_t *)pvParameters;
    esp_err_t err = perform_ota_update(params->update_url);
    if (err == ESP_OK) {
        err = save_firmware_version(params->new_version);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "Failed to save new firmware version");
        }
        
        ESP_LOGI(TAG, "OTA update succeeded, preparing to restart...");
        vTaskDelay(pdMS_TO_TICKS(2000)); // Short delay to allow logs to be printed
        esp_restart();
    } else {
        ESP_LOGE(TAG, "OTA update failed: %s", esp_err_to_name(err));
    }
    free(params);
    xSemaphoreGive(ota_semaphore); // Move this before vTaskDelete
    xTaskNotifyGive(xTaskGetCurrentTaskHandle());
    vTaskDelete(NULL);
}

void start_ota_update(const char *update_url, const char *new_version) {
    // Remove semaphore take from here since we already have it from check_ota_updates
    ota_update_params_t *params = malloc(sizeof(ota_update_params_t));
    if (params == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for OTA update parameters");
        xSemaphoreGive(ota_semaphore);
        return;
    }
    strncpy(params->update_url, update_url, sizeof(params->update_url) - 1);
    params->update_url[sizeof(params->update_url) - 1] = '\0';
    strncpy(params->new_version, new_version, sizeof(params->new_version) - 1);
    params->new_version[sizeof(params->new_version) - 1] = '\0';
    
    if (xTaskCreate(&ota_update_task, "ota_update_task", OTA_UPDATE_TASK_STACK_SIZE,
                        params, OTA_UPDATE_TASK_PRIORITY, NULL) == pdPASS) {
            // Wait for OTA task completion
            ulTaskNotifyTake(pdTRUE, portMAX_DELAY);
    } else {
        ESP_LOGE(TAG, "Failed to create OTA update task");
        free(params);
        xSemaphoreGive(ota_semaphore);
    }
}

void check_ota_updates(void) {
    if (ota_semaphore == NULL) {
        ESP_LOGE(TAG, "OTA semaphore not initialized");
        return;
    }

    if (xSemaphoreTake(ota_semaphore, pdMS_TO_TICKS(5000)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take OTA semaphore (timeout)");
        return;
    }

    char firmware_url[256];
    char new_version[32];
    esp_err_t err = check_for_updates(firmware_url, sizeof(firmware_url), new_version, sizeof(new_version));
    if (err == ESP_OK) {
        start_ota_update(firmware_url, new_version);
    } else {
        // If no update is needed, give back the semaphore
        xSemaphoreGive(ota_semaphore);
    }
}


void ota_init() {
    // Create binary semaphore
    ota_semaphore = xSemaphoreCreateBinary();
    if (ota_semaphore == NULL) {
        ESP_LOGE(TAG, "Failed to create OTA semaphore");
        return;
    }
    xSemaphoreGive(ota_semaphore); // Initialize it as available

    char version[32];
    esp_err_t ret = load_firmware_version(version, sizeof(version));
    if (ret == ESP_OK) {
        strncpy(current_firmware_version, version, sizeof(current_firmware_version) - 1);
        current_firmware_version[sizeof(current_firmware_version) - 1] = '\0';
    } else if (ret == ESP_ERR_NVS_NOT_FOUND) {
        ESP_LOGW(TAG, "Firmware version not found in NVS, setting default version");
        strncpy(current_firmware_version, DEFAULT_FIRMWARE_VERSION, sizeof(current_firmware_version) - 1);
        current_firmware_version[sizeof(current_firmware_version) - 1] = '\0';
        save_firmware_version(current_firmware_version);
    } else {
        ESP_LOGE(TAG, "Error loading firmware version: %s", esp_err_to_name(ret));
        strncpy(current_firmware_version, DEFAULT_FIRMWARE_VERSION, sizeof(current_firmware_version) - 1);
        current_firmware_version[sizeof(current_firmware_version) - 1] = '\0';
    }
    
    ESP_LOGI(TAG, "========= RUNNING FIRMWARE VERSION: %s =========", current_firmware_version);
    

    // Rest of the function remains the same...
}