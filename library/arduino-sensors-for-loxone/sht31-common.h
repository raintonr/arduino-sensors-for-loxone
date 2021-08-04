#ifndef SHT31_COMMON_H
#define SHT31_COMMON_H

/*
 * Choose which library to use for I2C communications:
 * - Adafruit: uses hardware I2C pins
 * - Bitbang: pick any pins you like
 */

#define USE_ADAFRUIT_SHT31

#ifdef USE_ADAFRUIT_SHT31
    #include <Adafruit_SHT31.h>
    Adafruit_SHT31 sht31;
#endif

#include <DS2438.h>

void init_sht31();
void read_sht31(float *temperature, float *humidity);
void zero_sht31_1w(DS2438 *device);
void set_sht31_humidity(DS2438 *device, float humidity);

#endif