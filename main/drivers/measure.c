#include "measure.h"
#include "config.h"        // Ensure this file defines BAT_VOL, SOL_VOL, VOL_EN
#include "driver/adc.h"
#include "driver/gpio.h"
#include "esp_adc_cal.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "inttypes.h"

#define TAG "MEASURE"

// ADC calibration
static esp_adc_cal_characteristics_t *adc_chars;
#define DEFAULT_VREF    3300        // Default VREF in mV
#define NO_OF_SAMPLES   64          // Multisampling

void measure_init(void)
{
    // Configure VOL_EN as already set in config.h
    // Ensure VOL_EN is set as output in config.h

    // Configure ADC
    adc1_config_width(ADC_WIDTH_BIT_12); // 12-bit resolution

    // Set attenuation to 11 dB to support up to ~3.9V after voltage divider
    adc1_config_channel_atten(BAT_VOL, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(SOL_VOL, ADC_ATTEN_DB_11);

    // Characterize ADC
    adc_chars = calloc(1, sizeof(esp_adc_cal_characteristics_t));
    if (adc_chars == NULL) {
        ESP_LOGE(TAG, "Failed to allocate memory for ADC characteristics");
        return;
    }

    esp_adc_cal_value_t val_type = esp_adc_cal_characterize(
        ADC_UNIT_1,
        ADC_ATTEN_DB_11,
        ADC_WIDTH_BIT_12,
        DEFAULT_VREF,
        adc_chars
    );

    if (val_type == ESP_ADC_CAL_VAL_EFUSE_TP) {
        ESP_LOGI(TAG, "ADC calibrated using Two Point values from eFuse");
    } else if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
        ESP_LOGI(TAG, "ADC calibrated using VREF from eFuse");
    } else {
        ESP_LOGI(TAG, "ADC calibrated using default VREF");
    }

    ESP_LOGI(TAG, "Measurement initialization complete.");
}

static uint32_t read_voltage(adc1_channel_t channel)
{
    uint32_t adc_reading = 0;

    // Multisampling
    for (int i = 0; i < NO_OF_SAMPLES; i++) {
        adc_reading += adc1_get_raw(channel);
    }
    adc_reading /= NO_OF_SAMPLES;

    return esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);
    // Convert adc_reading to voltage in mV
   //uint32_t voltage = esp_adc_cal_raw_to_voltage(adc_reading, adc_chars);

    // Round to 2 decimal places
   // float original_voltage = roundf(((float)voltage / 1000.0) * (4.0 / 3.0) * 100) / 100;

    //return voltage;
}

uint32_t get_bat_vol(void)
{
    // Enable voltage measurement
    gpio_set_level(VOL_EN, 1);
    vTaskDelay(pdMS_TO_TICKS(10)); // Wait for voltage to stabilize

    // Read battery voltage
    uint32_t voltage = read_voltage(BAT_VOL);

    // Disable voltage measurement if necessary
    gpio_set_level(VOL_EN, 0);

    return voltage;
}

uint32_t get_solar_vol(void)
{
    // Enable voltage measurement
    gpio_set_level(VOL_EN, 1);
    vTaskDelay(pdMS_TO_TICKS(10)); // Wait for voltage to stabilize

    // Read solar voltage
    uint32_t voltage = read_voltage(SOL_VOL);

    // Disable voltage measurement if necessary
    gpio_set_level(VOL_EN, 0);

    return voltage;
}

