#ifndef MEASURE_H
#define MEASURE_H

#include "inttypes.h"
#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Initialize ADC channels and necessary GPIO configurations.
 */
void measure_init(void);

/**
 * @brief Get the Battery Voltage.
 * 
 * @return float Battery voltage in volts.
 */
uint32_t get_bat_vol(void);

/**
 * @brief Get the Solar Voltage.
 * 
 * @return float Solar voltage in volts.
 */
uint32_t get_solar_vol(void);

#ifdef __cplusplus
}
#endif

#endif // MEASURE_H
