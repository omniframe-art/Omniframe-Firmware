#include "config.h"
#include "tps.h"
#include <sdkconfig.h>
#include "esp_err.h"
#include "esp_log.h"
#include <driver/gpio.h>
#include <driver/i2c_master.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include "nvs_flash.h"
#include "button.h"

#include "../components/epdiy/src/output_lcd/lcd_driver.h"
#include "../components/epdiy/src/epd_internals.h"
#include "../components/epdiy/src/render.h"
#include "../components/epdiy/src/epd_board.h"
#include "../components/epdiy/src/epd_highlevel.h"

tps_config_reg config_reg;
epd_ctrl_state_t ctrl_state;
EpdiyHighlevelState hl_state;
LcdEpdConfig_t lcd_config;
TPS_t tps;
int32_t SERIAL_NUMBER;  // Definition
int32_t VCOM;  // Definition

static lcd_bus_config_t lcd_bus = {
    .clock = XCL,
    .ckv = CKV,
    .leh = XLE,
    .start_pulse = XSTL,
    .stv = SPV,
    .data_0 = D0,
    .data_1 = D1,
    .data_2 = D2,
    .data_3 = D3,
    .data_4 = D4,
    .data_5 = D5,
    .data_6 = D6,
    .data_7 = D7,
    .data_8 = D8,
    .data_9 = D9,
    .data_10 = D10,
    .data_11 = D11,
    .data_12 = D12,
    .data_13 = D13,
    .data_14 = D14,
    .data_15 = D15,
};

const Display display = {
    .width = 1600,
    .height = 1200,
    .bus_width = 8,
    .bus_speed = 16,
    .default_waveform = &epdiy_ED097TC2,
    .display_type = 1,
};

void board_init() {
   // ESP_LOGI("BOARD INIT:", "hello");
    esp_log_level_set("gpio", ESP_LOG_WARN);

    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    io_conf.pin_bit_mask = (1ULL<<D0) | (1ULL<<D1) | (1ULL<<D2) | (1ULL<<D3) | 
                           (1ULL<<D4) | (1ULL<<D5) | (1ULL<<D6) | (1ULL<<D7) | 
                           (1ULL<<D8) | (1ULL<<D9) | (1ULL<<D10) | (1ULL<<D11) | 
                           (1ULL<<D12) | (1ULL<<D13) | (1ULL<<D14) | (1ULL<<D15) | 
                           (1ULL<<XLE) | (1ULL<<XCL) | (1ULL<<SPV) | (1ULL<<CKV) | 
                           (1ULL<<XOE) | (1ULL<<XSTL) | (1ULL<<MODE) | (1ULL<<VOL_EN);
    gpio_config(&io_conf);

    vTaskDelay(5);

    gpio_reset_pin(SD_EN);
    io_conf.pull_down_en = 1;
    io_conf.pin_bit_mask = (1ULL<<SD_EN);
    gpio_config(&io_conf);

    vTaskDelay(5);

    get_vcom_from_nvs();
    get_serial_number_from_nvs();

    ESP_ERROR_CHECK(tps_setup(&tps));

    tps_pwr_good();
    ESP_ERROR_CHECK(tps_write(&tps, TPS_REG_ENABLE, 0x3F));

    ctrl_reg_init();

    tps_sleep();
    tps_powerdown();
   // ESP_LOGI("BOARD INIT:", "done");
}

bool already_initialized = false;

void renderer_init(){

    if (already_initialized) {
        return;
    }

    lcd_config.pixel_clock = 16 * 1000 * 1000;
    lcd_config.ckv_high_time = 60;
    lcd_config.line_front_porch = 4;
    lcd_config.le_high_time = 4;
    lcd_config.bus_width = 8;
    lcd_config.bus = lcd_bus;

    epd_lcd_init(&lcd_config, display.width, display.height);
    epd_renderer_init();
    hl_state = epd_hl_init(display.default_waveform);

    already_initialized = true;
}

void ctrl_reg_init() {
    ctrl_state.ep_latch_enable = false;
    ctrl_state.ep_output_enable = false;
    ctrl_state.ep_sth = true;
    ctrl_state.ep_mode = false;
    ctrl_state.ep_stv = true;
    epd_ctrl_state_t mask = {
        .ep_latch_enable = true,
        .ep_output_enable = true,
        .ep_sth = true,
        .ep_mode = true,
        .ep_stv = true,
    };

    set_ctrl(&ctrl_state, &mask);
}

void set_mode(bool state) {
  ctrl_state.ep_output_enable = state;
  ctrl_state.ep_mode = state;
  epd_ctrl_state_t mask = {
    .ep_output_enable = true,
    .ep_mode = true,
  };
  set_ctrl(&ctrl_state, &mask);
  vTaskDelay(1);
}

void set_ctrl(epd_ctrl_state_t *state, const epd_ctrl_state_t *const mask) {
    if (mask->ep_output_enable || mask->ep_mode || mask->ep_stv) {
        if (state->ep_output_enable) {
            gpio_set_level(XOE, state->ep_output_enable ? 1 : 0);
        }
        if (state->ep_mode) {
            gpio_set_level(MODE, state->ep_mode ? 1 : 0);
        }
       // if (mask->ep_stv) {
        // gpio_set_level(SPV, state->ep_stv ? 1 : 0); 
       // }

        //TPS
        if (config_reg.pwrup) {
            gpio_set_level(POWER_UP, config_reg.pwrup ? 1 : 0);
        }
        if (config_reg.vcom_ctrl) {
            gpio_set_level(VCOM_CTRL, config_reg.vcom_ctrl ? 1 : 0);
        }
        if (config_reg.wakeup) {
            gpio_set_level(WAKEUP, config_reg.wakeup ? 1 : 0);
        }
    }
}

void board_poweron(epd_ctrl_state_t *state) {

    tps_wakeup();
    vTaskDelay(10);
   // ESP_LOGI("POWERON", "hello");

    epd_ctrl_state_t mask = {
        .ep_output_enable = true,
        .ep_mode = true,
        .ep_stv = true,
    };
    state->ep_stv = true;
    state->ep_mode = false;
    state->ep_output_enable = true;
    config_reg.wakeup = true;
    set_ctrl(state, &mask);
    config_reg.pwrup = true;
    set_ctrl(state, &mask);
    config_reg.vcom_ctrl = true;
    set_ctrl(state, &mask);
    vTaskDelay(10);

    ESP_ERROR_CHECK(tps_write(&tps, TPS_REG_ENABLE, 0x3F));
    ESP_ERROR_CHECK(tps_set_vcom(&tps, 2270));

    vTaskDelay(1);

    state->ep_sth = true;
    mask = (const epd_ctrl_state_t){
        .ep_sth = true,
    };
    set_ctrl(state, &mask);

    int tries = 0;
    uint8_t pg_status;
    esp_err_t ret;

    while (1) {
        ret = tps_read(&tps, TPS_REG_PG, &pg_status, 1);
        if (ret != ESP_OK) {
            ESP_LOGE("epdiy", "Failed to read PG status: %s", esp_err_to_name(ret));
            return;
        }

        if ((pg_status & 0xFA) == 0xFA) {
            break;  // Power good status achieved
        }

        if (tries >= 500) {
            ESP_LOGE("epdiy", "Power enable failed! PG status: 0x%02X", pg_status);
            return;
        }

        tries++;
        vTaskDelay(pdMS_TO_TICKS(1));  // 1ms delay between attempts
    }

    //ESP_LOGI("epdiy", "Power good status achieved after %d tries", tries);

    ESP_LOGI("POWERON", "done");
    vTaskDelay(1);
}

void board_poweroff(epd_ctrl_state_t *state) {
 //   ESP_LOGI("Poweroff", "hello");
  epd_ctrl_state_t mask = {
    .ep_stv = true,
    .ep_output_enable = true,
    .ep_mode = true,
  };
  config_reg.vcom_ctrl = false;
  config_reg.pwrup = false;
  state->ep_stv = false;
  state->ep_output_enable = false;
  state->ep_mode = false;
  set_ctrl(state, &mask);
  vTaskDelay(1);
  config_reg.wakeup = false;
  set_ctrl(state, &mask);

  tps_sleep();
  tps_powerdown();
  
    ESP_LOGI("Poweroff", "done");
}

void get_vcom_from_nvs(){
    esp_err_t err;
    nvs_handle_t nvs_handle;
    int32_t vcom_value;  // Changed to int32_t

    // Open NVS
    err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        // Read VCOM value using nvs_get_i32
        err = nvs_get_i32(nvs_handle, "vcom", &vcom_value);
        if (err == ESP_OK) {
            VCOM = vcom_value;
          }
        else{
            VCOM = 2270;
            ESP_LOGW("NVS", "VCOM not in NVS, using hardcoded value: %ld", VCOM);
            nvs_set_i32(nvs_handle, "vcom", VCOM);
            nvs_commit(nvs_handle);
            ESP_LOGI("NVS", "VCOM set to hardcoded value: %ld", VCOM);
        }
        ESP_LOGI("NVS", "VCOM: %ld", VCOM);
        // Close NVS
        nvs_close(nvs_handle);
    }
}

void get_serial_number_from_nvs(){
    esp_err_t err;
    nvs_handle_t nvs_handle;
    int32_t serial_num;  // Changed to int32_t

    // Open NVS
    err = nvs_open("storage", NVS_READWRITE, &nvs_handle);
    if (err == ESP_OK) {
        err = nvs_get_i32(nvs_handle, "serial", &serial_num);
    //     if (err == ESP_OK) {
    //         SERIAL_NUMBER = serial_num;
    //    }
       // else{
            SERIAL_NUMBER = 3;
            ESP_LOGW("NVS", "SERIAL not in NVS, using hardcoded value: %ld", SERIAL_NUMBER);
            nvs_set_i32(nvs_handle, "serial", SERIAL_NUMBER);
            nvs_commit(nvs_handle);
            ESP_LOGI("NVS", "SERIAL set to hardcoded value: %ld", SERIAL_NUMBER);
        //}
        ESP_LOGI("NVS", "SERIAL NUMBER: %ld", SERIAL_NUMBER);
        // Close NVS
        nvs_close(nvs_handle);
    }
}




