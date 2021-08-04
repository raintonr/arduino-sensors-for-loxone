#ifndef SHT31_COMMON_H
#define SHT31_COMMON_H

#include <DS2438.h>

void init_sht31();
void read_sht31(float *temperature, float *humidity);
void zero_sht31_1w(DS2438 *device);
void set_sht31_humidity(DS2438 *device, float humidity);

#endif