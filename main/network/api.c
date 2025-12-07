#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_http_client.h"
#include "esp_mac.h"
#include "lwip/inet.h"
#include "cJSON.h"
#include "api.h"
#include "config.h"
#include "measure.h"
#include "image_queue.h"
#include "../components/epdiy/src/output_common/lut.h"

static const char *TAG = "API";

static char json_response_buffer[JSON_RESPONSE_BUFFER_SIZE];
static int json_buffer_index = 0;

static uint8_t waveform_buffer[WAVEFORM_SIZE];
static size_t waveform_buffer_index = 0;

// Waveform HTTP handler
static esp_err_t waveform_http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_DATA:
            if (evt->data_len + waveform_buffer_index <= WAVEFORM_SIZE) {
                memcpy(waveform_buffer + waveform_buffer_index, evt->data, evt->data_len);
                waveform_buffer_index += evt->data_len;
            } else {
                ESP_LOGE(TAG, "Waveform buffer overflow");
                return ESP_FAIL;
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            if (waveform_buffer_index == WAVEFORM_SIZE) {
                size_t index = 0;
                for (int i = 0; i < SHADES; i++) {
                    for (int j = 0; j < FRAMES; j++) {
                        custom_wave[i][j] = waveform_buffer[index++];
                    }
                }
                save_waveform();
                ESP_LOGI(TAG, "Waveform data successfully received and loaded");
            } else {
                ESP_LOGE(TAG, "Incorrect waveform size received: %d bytes", waveform_buffer_index);
                return ESP_FAIL;
            }
            break;
        case HTTP_EVENT_DISCONNECTED:
            waveform_buffer_index = 0;
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t get_waveform(void) {
    ESP_LOGI(TAG, "Getting waveform");

    char url[256];
    snprintf(url, sizeof(url), "%s/get-waveform", BASE_URL);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = waveform_http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    waveform_buffer_index = 0;

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to make GET request: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}

// JSON HTTP handler for check_updates
static esp_err_t json_http_event_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ERROR:
            ESP_LOGI(TAG, "HTTP_EVENT_ERROR");
            break;
        case HTTP_EVENT_ON_DATA:
            if (evt->data_len + json_buffer_index < sizeof(json_response_buffer)) {
                memcpy(json_response_buffer + json_buffer_index, evt->data, evt->data_len);
                json_buffer_index += evt->data_len;
            } else {
                ESP_LOGE(TAG, "Response buffer overflow");
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            json_response_buffer[json_buffer_index] = '\0';
            cJSON *json = cJSON_Parse(json_response_buffer);
            if (json == NULL) {
                ESP_LOGE(TAG, "Failed to parse JSON response: %s", cJSON_GetErrorPtr());
                return ESP_FAIL;
            }

            cJSON *image_que = cJSON_GetObjectItem(json, "image_que");
            if (cJSON_IsArray(image_que)) {
                int array_size = cJSON_GetArraySize(image_que);
                memset(new_queue, 0, sizeof(new_queue));

                for (int i = 0; i < QUEUE_SIZE; i++) {
                    if (i < array_size) {
                        cJSON *item = cJSON_GetArrayItem(image_que, i);
                        if (cJSON_IsString(item)) {
                            strncpy(new_queue[i], item->valuestring, MAX_FILENAME_LENGTH - 1);
                            new_queue[i][MAX_FILENAME_LENGTH - 1] = '\0';
                        } else {
                            ESP_LOGE(TAG, "Invalid queue entry format at index %d", i);
                        }
                    } else {
                        new_queue[i][0] = '\0';
                    }
                }
                compare_queues();
            }
            cJSON_Delete(json);
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t check_updates(void) {
    ESP_LOGI(TAG, "Checking Updates");

    char url[256];
    snprintf(url, sizeof(url), "%s/check-updates/%ld", BASE_URL, SERIAL_NUMBER);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = json_http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    json_buffer_index = 0;

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to make GET request: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}

// Sync handler
static esp_err_t sync_handler(esp_http_client_event_t *evt) {
    switch(evt->event_id) {
        case HTTP_EVENT_ON_DATA:
            if (evt->data_len + json_buffer_index < sizeof(json_response_buffer)) {
                memcpy(json_response_buffer + json_buffer_index, evt->data, evt->data_len);
                json_buffer_index += evt->data_len;
            } else {
                ESP_LOGE(TAG, "Response buffer overflow");
            }
            break;
        case HTTP_EVENT_ON_FINISH:
            json_response_buffer[json_buffer_index] = '\0';
            cJSON *json = cJSON_Parse(json_response_buffer);
            if (json == NULL) {
                ESP_LOGE(TAG, "Failed to parse JSON response");
                return ESP_FAIL;
            }

            cJSON *queue = cJSON_GetObjectItem(json, "queue");
            if (queue && cJSON_IsArray(queue)) {
                int array_size = cJSON_GetArraySize(queue);
                memset(new_queue, 0, sizeof(new_queue));

                for (int i = 0; i < QUEUE_SIZE; i++) {
                    if (i < array_size) {
                        cJSON *item = cJSON_GetArrayItem(queue, i);
                        if (cJSON_IsString(item)) {
                            strncpy(new_queue[i], item->valuestring, MAX_FILENAME_LENGTH - 1);
                            new_queue[i][MAX_FILENAME_LENGTH - 1] = '\0';
                        } else {
                            ESP_LOGE(TAG, "Invalid queue entry format at index %d", i);
                        }
                    } else {
                        new_queue[i][0] = '\0';
                    }
                }
                compare_queues();
            }
            cJSON_Delete(json);
            break;
        case HTTP_EVENT_ERROR:
            ESP_LOGE(TAG, "HTTP_EVENT_ERROR");
            break;
        default:
            break;
    }
    return ESP_OK;
}

esp_err_t server_sync(void) {
    ESP_LOGI(TAG, "Syncing...");

    char url[256];
    snprintf(url, sizeof(url), "%s/sync", BASE_URL);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddNumberToObject(root, "batVol", get_bat_vol());
    cJSON_AddNumberToObject(root, "solVol", get_solar_vol());
    cJSON_AddStringToObject(root, "CurImg", "NULL");

    char *post_data = cJSON_Print(root);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
        .event_handler = sync_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    char serial_header[32];
    snprintf(serial_header, sizeof(serial_header), "%ld", SERIAL_NUMBER);
    esp_http_client_set_header(client, "Serial", serial_header);
    esp_http_client_set_header(client, "Content-Type", "application/json");
    esp_http_client_set_post_field(client, post_data, strlen(post_data));

    json_buffer_index = 0;
    esp_err_t err = esp_http_client_perform(client);

    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to make POST request: %s", esp_err_to_name(err));
    }

    cJSON_Delete(root);
    free(post_data);
    esp_http_client_cleanup(client);
    return err;
}

esp_err_t send_device_info(void) {
    char post_data[150];
    uint8_t mac[6];
    esp_read_mac(mac, ESP_MAC_WIFI_STA);

    esp_netif_t *netif = esp_netif_get_default_netif();
    esp_netif_ip_info_t ip_info;

    esp_err_t result = esp_netif_get_ip_info(netif, &ip_info);
    if (result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get IP info: %s", esp_err_to_name(result));
        return result;
    }

    extern char current_firmware_version[];
    snprintf(post_data, sizeof(post_data),
             "{\"serial\": %ld, \"ip\": \"%s\", \"version\": \"%s\"}",
             SERIAL_NUMBER,
             ip4addr_ntoa((const ip4_addr_t*)&ip_info.ip),
             current_firmware_version);

    char url[256];
    snprintf(url, sizeof(url), "%s/connect", BASE_URL);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_POST,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    esp_http_client_set_post_field(client, post_data, strlen(post_data));
    esp_http_client_set_header(client, "Content-Type", "application/json");

    esp_err_t err = esp_http_client_perform(client);
    if (err == ESP_OK) {
        ESP_LOGI(TAG, "HTTP POST Status = %d, content_length = %lld",
                 esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
    } else {
        ESP_LOGE(TAG, "Sending device info failed: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}

esp_err_t hello(void) {
    char url[256];
    snprintf(url, sizeof(url), "%s/hello", BASE_URL);

    esp_http_client_config_t config = {
        .url = url,
        .method = HTTP_METHOD_GET,
        .event_handler = json_http_event_handler,
    };
    esp_http_client_handle_t client = esp_http_client_init(&config);

    json_buffer_index = 0;

    esp_err_t err = esp_http_client_perform(client);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "Failed to make GET request: %s", esp_err_to_name(err));
    }

    esp_http_client_cleanup(client);
    return err;
}

