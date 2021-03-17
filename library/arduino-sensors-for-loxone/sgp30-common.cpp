#include <onewire-common.h>
#include <sgp30-common.h>

void init_sgp30(Adafruit_SGP30 *sensor) {
  // In case sensor is not connected at startup, loop round looking for it
  while (!sensor->begin()) {
    Serial.println("Couldn't find SGP30");
    error_flash(500);
  }
#ifdef DEBUG
  Serial.print("Found SGP30 serial #");
  Serial.print(sensor->serialnumber[0], HEX);
  Serial.print(sensor->serialnumber[1], HEX);
  Serial.println(sensor->serialnumber[2], HEX);
#endif
}

void zero_sgp30_1w(DS2438 *device) {
  // Zero out, but according to what we think 'zero' is ;)
  device->setTemperature((int8_t)0);
  device->setVDDVoltage((uint16_t)0);
  device->setVADVoltage((uint16_t)0);
  device->setCurrent((int16_t)-1023);
}