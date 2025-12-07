#ifndef CONFIG_H
#define CONFIG_H

#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "../components/epdiy/src/epd_internals.h" // Adjust path as necessary
#include "../components/epdiy/src/epd_board.h"
#include "../components/epdiy/src/epd_highlevel.h"
#include "../components/epdiy/src/output_lcd/lcd_driver.h"
#include "tps.h"
#include <inttypes.h>

///*
//BOARD V4 GPIOS
#define D0        GPIO_NUM_36
#define D1        GPIO_NUM_38
#define D2        GPIO_NUM_37
#define D3        GPIO_NUM_39
#define D4        GPIO_NUM_40
#define D5        GPIO_NUM_42
#define D6        GPIO_NUM_41
#define D7        GPIO_NUM_44
#define D8        -1
#define D9        -1
#define D10       -1
#define D11       -1
#define D12       -1
#define D13       -1
#define D14       -1
#define D15       -1
#define XLE       GPIO_NUM_45 //Latch enable, leh
#define XCL       GPIO_NUM_48 //Clock
#define SPV       GPIO_NUM_2 //STV?
#define CKV       GPIO_NUM_1 //CKV
#define XOE       GPIO_NUM_7 //Output enable
#define XSTL      GPIO_NUM_35 //STH
#define MODE      GPIO_NUM_43
#define BTN       GPIO_NUM_4

#define BAT_VOL   ADC1_CHANNEL_2 //GPIO_NUM_18
//#define BAT_VOL   GPIO_NUM_3
#define VOL_EN    GPIO_NUM_46
#define SOL_VOL   ADC1_CHANNEL_8 //GPIO_NUM_9
//#define SOL_VOL   GPIO_NUM_9

#define PWR_GOOD  GPIO_NUM_10
#define POWER_UP  GPIO_NUM_11
#define SDA       GPIO_NUM_12
#define SCL       GPIO_NUM_13
#define VCOM_CTRL GPIO_NUM_14
#define WAKEUP    GPIO_NUM_21
#define NINT      GPIO_NUM_47
//*/
#define SD_EN  GPIO_NUM_0
#define SD_CMD GPIO_NUM_16
#define SD_CLK GPIO_NUM_17
#define SD_D0  GPIO_NUM_8
#define SD_D1  GPIO_NUM_18
#define SD_D2  GPIO_NUM_5
#define SD_D3  GPIO_NUM_6
//////


/*
//V3 GPIOS
#define D0        GPIO_NUM_35
#define D1        GPIO_NUM_36
#define D2        GPIO_NUM_37
#define D3        GPIO_NUM_38
#define D4        GPIO_NUM_39
#define D5        GPIO_NUM_41
#define D6        GPIO_NUM_40
#define D7        GPIO_NUM_42
#define D8        GPIO_NUM_6
#define D9        GPIO_NUM_6
#define D10       GPIO_NUM_6
#define D11       GPIO_NUM_6
#define D12       GPIO_NUM_6
#define D13       GPIO_NUM_6
#define D14       GPIO_NUM_6
#define D15       GPIO_NUM_6
#define XLE       GPIO_NUM_47 //Latch enable, leh
#define XCL       GPIO_NUM_21 //Clock
#define SPV       GPIO_NUM_6 //STV?
#define CKV       GPIO_NUM_1 //CKV
#define XOE       GPIO_NUM_48 //Output enable
#define XSTL      GPIO_NUM_45 //STH
#define MODE      GPIO_NUM_7
#define BORDER    GPIO_NUM_2
#define BTN       GPIO_NUM_4
#define BQ_EN     GPIO_NUM_17
#define BAT_VOL   ADC2_CHANNEL_7 //GPIO_NUM_18
//#define BAT_VOL   GPIO_NUM_18

#define PWR_SWITCH GPIO_NUM_5
#define VOL_EN    GPIO_NUM_18
#define SOL_VOL   ADC1_CHANNEL_2 //GPIO_NUM_3
//#define SOL_VOL   GPIO_NUM_3

#define PWR_GOOD  GPIO_NUM_46
#define POWER_UP  GPIO_NUM_9
#define SDA       GPIO_NUM_10
#define SCL       GPIO_NUM_11
#define VCOM_CTRL GPIO_NUM_12
#define WAKEUP    GPIO_NUM_13
#define NINT      GPIO_NUM_14

*/
#define I2C_PORT  I2C_NUM_0

extern int32_t VCOM;
extern int32_t SERIAL_NUMBER;
extern const int que_len;
extern const int lut_size;

typedef struct {
    i2c_port_t port;
    bool pwrup;
    bool vcom_ctrl;
    bool wakeup;
    bool nint;
    bool pwr_good;
} tps_config_reg;

extern epd_ctrl_state_t ctrl_state;
extern tps_config_reg config_reg;
extern EpdiyHighlevelState hl_state;
extern LcdEpdConfig_t lcd_config_2;

typedef struct {
    /// Width of the display in pixels.
    int width;
    /// Height of the display in pixels.
    int height;

    /// Width of the data bus in bits.
    uint8_t bus_width;
    /// Speed of the data bus in MHz, if configurable.
    int bus_speed;

    /// Default waveform to use.
    const EpdWaveform* default_waveform;
    /// Display type
    int display_type; // Changed from `enum display_type` to `int` for simplicity, adjust as necessary
} Display;

extern const Display display;

// Function prototypes
void board_init();
void renderer_init();
//static void IRAM_ATTR interrupt_handler(void* arg); // Static functions are not usually declared in header files
void ctrl_reg_init();
void set_ctrl(epd_ctrl_state_t *state, const epd_ctrl_state_t * const mask);
void board_poweron(epd_ctrl_state_t *state);
void board_poweroff(epd_ctrl_state_t *state);
void set_mode(bool state);
void get_vcom_from_nvs();
void get_serial_number_from_nvs();

#endif // CONFIG_H