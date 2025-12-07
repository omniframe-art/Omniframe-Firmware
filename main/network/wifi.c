#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "lwip/inet.h"
#include "cJSON.h"
#include "esp_flash_partitions.h"
#include "esp_partition.h"
#include "esp_heap_caps.h"
#include "esp_http_client.h"
#include "esp_mac.h"
#include "wifi.h"
#include "config.h"

#define EXAMPLE_ESP_MAXIMUM_RETRY  5
#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT      BIT1

#define NVS_NAMESPACE "storage"

static EventGroupHandle_t s_wifi_event_group;
static const char *TAG = "wifi station";
static int s_retry_num = 0;

#define MAX_SSID_LENGTH 32
#define MAX_PASSWORD_LENGTH 64
char wifi_ssid[MAX_SSID_LENGTH];
char wifi_password[MAX_PASSWORD_LENGTH];

static void event_handler(void* arg, esp_event_base_t event_base, int32_t event_id, void* event_data) {
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        if (s_retry_num < EXAMPLE_ESP_MAXIMUM_RETRY) {
            esp_wifi_connect();
            s_retry_num++;
            ESP_LOGI(TAG, "retry to connect to the AP");
        } else {
            xEventGroupSetBits(s_wifi_event_group, WIFI_FAIL_BIT);
        }
        ESP_LOGI(TAG,"connect to the AP fail");
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t* event = (ip_event_got_ip_t*) event_data;
        ESP_LOGI(TAG, "got ip:" IPSTR, IP2STR(&event->ip_info.ip));
        s_retry_num = 0;
        xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
    }
}

esp_err_t save_credentials(const char *ssid, const char *password) {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READWRITE, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE("Save Credents", "Failed to open NVS handle: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = nvs_set_str(nvs_handle, "wifi_ssid", ssid);
    if (ret == ESP_OK) {
        ret = nvs_set_str(nvs_handle, "wifi_password", password);
    }
    if (ret != ESP_OK) {
        ESP_LOGE("Save Credents", "Failed to set credentials in NVS: %s", esp_err_to_name(ret));
        nvs_close(nvs_handle);
        return ret;
    }

    ret = nvs_commit(nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE("Save Credents", "Failed to commit NVS: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI("Save Credents", "Successfully saved wifi credentials: %s | %s", ssid, password);
    }

    nvs_close(nvs_handle);
    return ret;
}

esp_err_t load_credentials() {
    nvs_handle_t nvs_handle;
    esp_err_t ret = nvs_open(NVS_NAMESPACE, NVS_READONLY, &nvs_handle);
    if (ret != ESP_OK) {
        ESP_LOGE("Load Credents", "Failed to open NVS handle: %s", esp_err_to_name(ret));
    }

    size_t ssid_size = sizeof(wifi_ssid);
    size_t password_size = sizeof(wifi_password);
    
    ret = nvs_get_str(nvs_handle, "wifi_ssid", wifi_ssid, &ssid_size);
    if (ret == ESP_OK) {
        ret = nvs_get_str(nvs_handle, "wifi_password", wifi_password, &password_size);
    }
    
    if (ret != ESP_OK) {
       
            ESP_LOGW("Load Credents", "Credentials not found in NVS, using defaults");
            strncpy(wifi_ssid, DEFAULT_WIFI_SSID, sizeof(wifi_ssid) - 1);
            strncpy(wifi_password, DEFAULT_WIFI_PASS, sizeof(wifi_password) - 1);
            save_credentials(DEFAULT_WIFI_SSID, DEFAULT_WIFI_PASS);
        
    } else {
        ESP_LOGI("Load Credents", "Successfully loaded wifi credentials");
    }

    nvs_close(nvs_handle);
    return ret;
}

void wifi_init_sta(void) {
    s_wifi_event_group = xEventGroupCreate();
    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));

    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    load_credentials();

    wifi_config_t wifi_config = {
        .sta = {
            .threshold.authmode = WIFI_AUTH_OPEN, 
        },
    };

    memcpy(wifi_config.sta.ssid, wifi_ssid, sizeof(wifi_config.sta.ssid));
    memcpy(wifi_config.sta.password, wifi_password, sizeof(wifi_config.sta.password));

    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
    ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE)); 
    ESP_ERROR_CHECK(esp_wifi_start());

    EventBits_t bits = xEventGroupWaitBits(s_wifi_event_group,
            WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
            pdFALSE,
            pdFALSE,
            portMAX_DELAY);

    if (bits & WIFI_CONNECTED_BIT) {
        ESP_LOGI(TAG, "connected to ap SSID:%s password:%s",
                 wifi_ssid, wifi_password);
    } else if (bits & WIFI_FAIL_BIT) {
        ESP_LOGI(TAG, "Failed to connect to SSID:%s, password:%s",
                 wifi_ssid, wifi_password);
    } else {
        ESP_LOGE(TAG, "UNEXPECTED EVENT");
    }
}
void nvs_init(){
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);
}

void setup_wifi() {
    esp_log_level_set("wifi", ESP_LOG_WARN);
    wifi_init_sta();
}


void disable_wifi() {
    esp_wifi_stop();
    esp_wifi_deinit();
}

void enable_wifi() {
    ESP_LOGI(TAG, "Enabling Wi-Fi...");

    // Reinitialize the Wi-Fi driver
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_wifi_init(&cfg);
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi initialized successfully.");
    } else {
        ESP_LOGE(TAG, "Failed to initialize Wi-Fi: %s", esp_err_to_name(ret));
        return;
    }

    // Register event handlers again
    esp_event_handler_instance_t instance_any_id;
    esp_event_handler_instance_t instance_got_ip;
    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_any_id));
    ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT,
                                                        IP_EVENT_STA_GOT_IP,
                                                        &event_handler,
                                                        NULL,
                                                        &instance_got_ip));

    // Start Wi-Fi
    ret = esp_wifi_start();
    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "Wi-Fi started successfully.");
    } else {
        ESP_LOGE(TAG, "Failed to start Wi-Fi: %s", esp_err_to_name(ret));
    }
}


