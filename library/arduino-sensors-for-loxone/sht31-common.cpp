#include <asfl-common.h>
#include <onewire-common.h>
#include <sht31-common.h>

void init_sht31(Adafruit_SHT31 *sensor) {
  // In case sensor is not connected at startup, loop round looking for it
  // Set to 0x45 for alternate i2c addr
  while (!sensor->begin(0x44)) {
#ifdef DEBUG
    Serial.println("Couldn't find SHT31");
#endif
    error_flash(500);
  }
}

void read_sht31(Adafruit_SHT31 *sensor, float *temperature, float *humidity) {
  // Read sensor in a loop until good reading is found. This way, if the SHT
  // sensor fails we will not respond to 1-Wire bus with bad data and master
  // will know something is wrong.
  boolean good_reading = false;
  do {
    *temperature = sensor->readTemperature();
    *humidity = sensor->readHumidity();
    if (isnan(*temperature) || isnan(*humidity)) {
      // Something bad - flash LED
      error_flash(500);
#ifdef DEBUG
      Serial.println("Failed to read sensor");
#endif
    } else {
      good_reading = true;
    }
  } while (!good_reading);
}

void zero_sht31_1w(DS2438 *device) {
  // Zero out, but according to what we think 'zero' is ;)
  device->setTemperature((int8_t)0);
  device->setVDDVoltage((uint16_t)512);
  device->setVADVoltage((uint16_t)512);
  device->setCurrent((int16_t)-1023);
}

void set_sht31_humidity(DS2438 *device, float humidity) {
  // Have to scale humidity to fit in current (between -1023 & +1023)
  // So divide multiply by 20.46 and subtract 1023
  // This ends up in Loxone between -0.25 & +0.25
  humidity *= 20.46;
  humidity -= 1023;
  int16_t humidity_raw = humidity;
#ifdef DEBUG
  Serial.print("\tDS2438 humidity: ");
  Serial.print(humidity_raw);
#endif
  device->setCurrent(humidity_raw);
}