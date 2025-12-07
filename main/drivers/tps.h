#ifndef TPS_H
#define TPS_H

#include <driver/i2c_master.h>
#include "esp_err.h"
#include "esp_log.h"
#include "config.h"

#define TPS_ADDRESS 0x68

#define TPS_REG_TMST_VALUE 0x00
#define TPS_REG_ENABLE 0x01
#define TPS_REG_VADJ 0x02
#define TPS_REG_VCOM1 0x03
#define TPS_REG_VCOM2 0x04
#define TPS_REG_INT_EN1 0x05
#define TPS_REG_INT_EN2 0x06
#define TPS_REG_INT1 0x07
#define TPS_REG_INT2 0x08
#define TPS_REG_UPSEQ0 0x09
#define TPS_REG_UPSEQ1 0x0A
#define TPS_REG_DWNSEQ0 0x0B
#define TPS_REG_DWNSEQ1 0x0C
#define TPS_REG_TMST1 0x0D
#define TPS_REG_TMST2 0x0E
#define TPS_REG_PG 0x0F
#define TPS_REG_REVID 0x10

typedef struct {
    i2c_master_bus_handle_t bus_handle;
    i2c_master_dev_handle_t dev_handle;
    i2c_port_t i2c_port;
} TPS_t;

esp_err_t tps_init(TPS_t *tps);
void tps_deinit(TPS_t *tps);
esp_err_t tps_setup(TPS_t *tps);
esp_err_t tps_write(TPS_t *tps, uint8_t reg, uint8_t data);
esp_err_t tps_read(TPS_t *tps, uint8_t reg, uint8_t *data, size_t size);
esp_err_t tps_set_vcom(TPS_t *tps, unsigned vcom);
void tps_powerup();
void tps_powerdown();
void tps_wakeup();
void tps_sleep();
void tps_pwr_good();
void tps_interrupt_handler(void *arg);

#endif // TPS_H
