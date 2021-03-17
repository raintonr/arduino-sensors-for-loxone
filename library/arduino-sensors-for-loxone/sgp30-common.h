#ifndef SGP30_COMMON_H
#define SGP30_COMMON_H

#include <DS2438.h>
#include <Adafruit_SGP30.h>

void init_sgp30(Adafruit_SGP30 *sensor);
void zero_sgp30_1w(DS2438 *device);

#endif