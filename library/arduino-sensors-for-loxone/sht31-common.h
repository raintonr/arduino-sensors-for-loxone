#ifndef SHT31_COMMON_H
#define SHT31_COMMON_H

#include <DS2438.h>
#include <Adafruit_SHT31.h>

void init_sht31(Adafruit_SHT31 *sensor);
void read_sht31(Adafruit_SHT31 *sensor, float *temperature, float *humidity);
void zero_sht31_1w(DS2438 *device);
void set_sht31_humidity(DS2438 *device, float humidity);

#endif