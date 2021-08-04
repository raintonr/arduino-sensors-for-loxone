#include <asfl-common.h>
#include <onewire-common.h>
#include <sht31-common.h>

/*
 * Choose which library to use for I2C communications:
 * - Adafruit: uses hardware I2C pins
 * - Bitbang: pick any pins you like
 */

//#define USE_ADAFRUIT_SHT31
#define USE_BITBANG_SHT31

#ifdef USE_ADAFRUIT_SHT31
#include <Adafruit_SHT31.h>
Adafruit_SHT31 sht31;
#endif

#ifdef USE_BITBANG_SHT31
#include <BitBang_I2C.h>
BBI2C bbi2c;
#define SHT31_I2C_ADDRESS 0x44
#define I2C_SDA_PIN 16
#define I2C_SCL_PIN 17
#define I2C_CLOCK_FREQUENCY 100000

// Stolen from Adafruit SHT31 code
uint8_t readbuffer[6];
uint8_t SHT31_MEAS_HIGHREP[] = {0x24, 0x00};
static uint8_t crc8(const uint8_t *data, int len) {
  /*
   *
   * CRC-8 formula from page 14 of SHT spec pdf
   *
   * Test data 0xBE, 0xEF should yield 0x92
   *
   * Initialization data 0xFF
   * Polynomial 0x31 (x8 + x5 +x4 +1)
   * Final XOR 0x00
   */

  const uint8_t POLYNOMIAL(0x31);
  uint8_t crc(0xFF);

  for (int j = len; j; --j) {
    crc ^= *data++;

    for (int i = 8; i; --i) {
      crc = (crc & 0x80) ? (crc << 1) ^ POLYNOMIAL : (crc << 1);
    }
  }
  return crc;
}

#endif /* USE_BITBANG_SHT31 */

void init_sht31() {
#ifdef DEBUG
  Serial.println("Init SHT31");
#endif /* DEBUG */

#ifdef USE_ADAFRUIT_SHT31
  // In case sensor is not connected at startup, loop round looking for it
  // Set to 0x45 for alternate i2c addr
  while (!sht31.begin(0x44)) {
#ifdef DEBUG
    Serial.println("Couldn't find SHT31");
#endif
    error_flash(500);
  }
#endif /* USE_ADAFRUIT_SHT31 */

#ifdef USE_BITBANG_SHT31
  memset(&bbi2c, 0, sizeof(bbi2c));
  bbi2c.iSDA = I2C_SDA_PIN;
  bbi2c.iSCL = I2C_SCL_PIN;
  I2CInit(&bbi2c, I2C_CLOCK_FREQUENCY);
  delay(100);  // allow devices to power up
  int discoveredDevice = I2CDiscoverDevice(&bbi2c, SHT31_I2C_ADDRESS);
#ifdef DEBUG
  Serial.print("Discovered device: ");
  Serial.println(discoveredDevice);
#endif /* DEBUG */
#endif /* USE_BITBANG_SHT31 */

#ifdef DEBUG
  Serial.println("SHT31 started");
#endif
}

void read_sht31(float *temperature, float *humidity) {
  // Make sure heater is always off
#ifdef USE_ADAFRUIT_SHT31
  if (sht31.isHeaterEnabled()) {
#ifdef DEBUG
    Serial.println("Whoa, SHT31 heater is on - turning off");
#endif
    sht31.heater(false);
    // And wait for 10s
    error_flash(10000);
  }
#endif

  // Read sensor in a loop until good reading is found. This way, if the SHT
  // sensor fails we will not respond to 1-Wire bus with bad data and master
  // will know something is wrong.
  boolean good_reading = false;
  do {
#ifdef USE_ADAFRUIT_SHT31
    *temperature = sht31.readTemperature();
    *humidity = sht31.readHumidity();
#endif /* USE_ADAFRUIT_SHT31 */

#ifdef USE_BITBANG_SHT31
    /* Measurement High Repeatability with Clock Stretch Disabled */
    I2CWrite(&bbi2c, SHT31_I2C_ADDRESS, SHT31_MEAS_HIGHREP,
             sizeof(SHT31_MEAS_HIGHREP));
    delay(20);
    if (I2CRead(&bbi2c, SHT31_I2C_ADDRESS, readbuffer, sizeof(readbuffer))) {
      // Stolen from Adafruit library
      if (readbuffer[2] != crc8(readbuffer, 2) ||
          readbuffer[5] != crc8(readbuffer + 3, 2)) {
      } else {
        int32_t stemp =
            (int32_t)(((uint32_t)readbuffer[0] << 8) | readbuffer[1]);
        // simplified (65536 instead of 65535) integer version of:
        // temp = (stemp * 175.0f) / 65535.0f - 45.0f;
        stemp = ((4375 * stemp) >> 14) - 4500;
        *temperature = (float)stemp / 100.0f;

        uint32_t shum = ((uint32_t)readbuffer[3] << 8) | readbuffer[4];
        // simplified (65536 instead of 65535) integer version of:
        // humidity = (shum * 100.0f) / 65535.0f;
        shum = (625 * shum) >> 12;
        *humidity = (float)shum / 100.0f;
      }
    }
#endif /* USE_BITBANG_SHT31 */

    if (isnan(*temperature) || isnan(*humidity)) {
      // Something bad - flash LED
      error_flash(500);
#ifdef DEBUG
      Serial.println("Failed to read SHT31");
#endif
    } else {
#ifdef DEBUG
      Serial.print("Read SHT31: ");
      Serial.print(*temperature);
      Serial.print(" ");
      Serial.println(*humidity);
#endif
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