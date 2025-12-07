#include "tps.h"
#include "config.h"
#include "freertos/FreeRTOS.h"
#include "driver/i2c_master.h"
#include "esp_log.h"

esp_err_t tps_init(TPS_t *tps) {
    tps->bus_handle = NULL;
    tps->dev_handle = NULL;
    tps->i2c_port = I2C_NUM_0;
    return ESP_OK;
}

void tps_deinit(TPS_t *tps) {
    if (tps->dev_handle) {
        i2c_master_bus_rm_device(tps->dev_handle);
    }
    if (tps->bus_handle) {
        i2c_del_master_bus(tps->bus_handle);
    }
}

esp_err_t tps_setup(TPS_t *tps) {
    esp_err_t ret;

    tps_init(tps);

    gpio_config_t io_conf_input = {
        .pin_bit_mask = (1ULL << PWR_GOOD) | (1ULL << NINT),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf_input));

    gpio_config_t io_conf_output = {
        .pin_bit_mask = (1ULL << POWER_UP) | (1ULL << WAKEUP) | (1ULL << VCOM_CTRL),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&io_conf_output));

    ESP_ERROR_CHECK(gpio_set_intr_type(NINT, GPIO_INTR_NEGEDGE));
    //ESP_ERROR_CHECK(gpio_install_isr_service(ESP_INTR_FLAG_EDGE));
    ESP_ERROR_CHECK(gpio_isr_handler_add(NINT, tps_interrupt_handler, NULL));

    tps_powerup();
    tps_wakeup();

    i2c_master_bus_config_t i2c_mst_config = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .i2c_port = tps->i2c_port,
        .scl_io_num = SCL,
        .sda_io_num = SDA,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = false,
    };

    ret = i2c_new_master_bus(&i2c_mst_config, &tps->bus_handle);
    if (ret != ESP_OK) {
        ESP_LOGE("TPS", "Failed to create I2C master bus: %s", esp_err_to_name(ret));
        return ret;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address = TPS_ADDRESS,
        .scl_speed_hz = 100000,
    };

    ret = i2c_master_bus_add_device(tps->bus_handle, &dev_cfg, &tps->dev_handle);
    if (ret != ESP_OK) {
        ESP_LOGE("TPS", "Failed to add TPS device: %s", esp_err_to_name(ret));
        return ret;
    }

    ESP_ERROR_CHECK(tps_write(tps, TPS_REG_ENABLE, 0x3F));

    uint8_t version;
    ret = tps_read(tps, 0x10, &version, 1);
    if (ret != ESP_OK) {
        ESP_LOGE("TPS", "TPS device not responding: %s", esp_err_to_name(ret));
        return ret;
    }
    //ESP_LOGI("TPS", "TPS device responding, version: 0x%02X", version);

   // ESP_LOGI("TPS", "TPS setup completed successfully");
    return ESP_OK;
}

esp_err_t tps_read(TPS_t *tps, uint8_t reg, uint8_t *data, size_t size) {
    esp_err_t ret;
    
    // Write the register address
    ret = i2c_master_transmit(tps->dev_handle, &reg, 1, -1);
    if (ret != ESP_OK) {
        ESP_LOGE("TPS", "Failed to write register address 0x%02X, error: %s", reg, esp_err_to_name(ret));
        return ret;
    }
    
    // Read the data
    ret = i2c_master_receive(tps->dev_handle, data, size, -1);
    if (ret != ESP_OK) {
        ESP_LOGE("TPS", "Failed to read from register 0x%02X, error: %s", reg, esp_err_to_name(ret));
    }
    return ret;
}


esp_err_t tps_write(TPS_t *tps, uint8_t reg, uint8_t data) {
    uint8_t write_buf[2] = {reg, data};
    esp_err_t ret = i2c_master_transmit(tps->dev_handle, write_buf, sizeof(write_buf), -1);
    if (ret != ESP_OK) {
        ESP_LOGE("TPS", "Failed to write 0x%02X to register 0x%02X, error: %s", data, reg, esp_err_to_name(ret));
    } 
    return ret;
}

esp_err_t tps_set_vcom(TPS_t *tps, unsigned vcom_mV) {
    //ESP_LOGI("TPS", "Setting VCOM to -%d mV", vcom_mV);
    unsigned val = vcom_mV / 10;
    esp_err_t ret;
    ret = tps_write(tps, 4, (val & 0x100) >> 8);
    if (ret != ESP_OK) return ret;
    return tps_write(tps, 3, val & 0xFF);
}

esp_err_t tps_read_thermistor(TPS_t *tps, int8_t *temperature) {
    esp_err_t ret = tps_write(tps, TPS_REG_TMST1, 0x80);
    if (ret != ESP_OK) return ret;

    int tries = 0;
    uint8_t val;
    while (tries < 100) {
        ret = tps_read(tps, TPS_REG_TMST1, &val, 1);
        if (ret != ESP_OK) return ret;
        if (val & 0x20) break;  // temperature conversion done
        tries++;
        vTaskDelay(pdMS_TO_TICKS(1));
    }

    if (tries >= 100) {
        ESP_LOGE("TPS", "Thermistor read timeout!");
        return ESP_ERR_TIMEOUT;
    }

    return tps_read(tps, TPS_REG_TMST_VALUE, (uint8_t*)temperature, 1);
}

void tps_powerup() {
    //ESP_LOGI("TPS", "Setting POWER_UP high");
    gpio_set_level(POWER_UP, 1);
    vTaskDelay(pdMS_TO_TICKS(1));
}

void tps_powerdown() {
    gpio_set_level(POWER_UP, 0);
}

void tps_wakeup() {
   // ESP_LOGI("TPS", "Setting WAKEUP high");
    gpio_set_level(WAKEUP, 1);
    vTaskDelay(pdMS_TO_TICKS(10));
}

void tps_sleep() {
    gpio_set_level(WAKEUP, 0);
}

void tps_pwr_good() {
   ESP_LOGI("POWER GOOD", "waiting for PWR_GOOD");
    while (gpio_get_level(PWR_GOOD) == 0) {
        printf(".");
        vTaskDelay(pdMS_TO_TICKS(10)); 
    }
    //ESP_LOGI("POWERON", "PG is up");
}

void IRAM_ATTR tps_interrupt_handler(void *arg) {
    // Handle interrupt logic here
}