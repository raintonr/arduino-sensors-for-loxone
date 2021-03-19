#include <EEPROM.h>
#include <onewire-common.h>
#include <sgp30-common.h>

// Bytes 0-7 are reserved for 1 wire address so put define EEPROM address to
// store baseline after that
#define BASELINE_ADDRESS 16
#define BASELINE_MARKER '@'

// Shouldn't this be defined in the Adafruit header? :(
#define SGP30_SERIAL_LENGTH 3

// Prints the sensor address to console
#ifdef DEBUG
void dump_sgp30_serial(const char *prefix,
                       const uint16_t serialnumber[SGP30_SERIAL_LENGTH],
                       const char *postfix) {
  Serial.print(prefix);

  for (int lp = 0; lp < SGP30_SERIAL_LENGTH; lp++) {
    // TODO: surely there's a better way to do this?
    if (lp > 0) Serial.print(":");
    Serial.print(serialnumber[lp] < 0xf ? "0" : "");
    Serial.print(serialnumber[lp] < 0xff ? "0" : "");
    Serial.print(serialnumber[lp] < 0xfff ? "0" : "");
    Serial.print(serialnumber[lp], HEX);
  }

  Serial.println(postfix);
}
#endif

// Global structure to store baseline and sensor serial
struct {
  uint16_t eCO2_base;
  uint16_t TVOC_base;
  uint16_t serialnumber[SGP30_SERIAL_LENGTH];
} baseline_data;

void init_sgp30(Adafruit_SGP30 *sensor) {
  // In case sensor is not connected at startup, loop round looking for it
  while (!sensor->begin()) {
    error_flash(500);
#ifdef DEBUG
    Serial.println("Couldn't find SGP30");
#endif
  }
#ifdef DEBUG
  dump_sgp30_serial("Found SGP30 serial #", sensor->serialnumber, "");
#endif

  int address = BASELINE_ADDRESS;
  if (EEPROM.read(address++) == BASELINE_MARKER) {
    // Looks like we have previously stored baseline available
    EEPROM.get(address, baseline_data);

    // Check the baseline stored matches current serial
    boolean serial_match = true;
    for (int lp = 0; lp < SGP30_SERIAL_LENGTH && serial_match; lp++) {
      if (baseline_data.serialnumber[lp] != sensor->serialnumber[lp]) {
        serial_match = false;
      }
    }

    if (serial_match) {
#ifdef DEBUG
      Serial.print("Setting saved baseline values: eCO2: 0x");
      Serial.print(baseline_data.eCO2_base, HEX);
      Serial.print(" & TVOC: 0x");
      Serial.println(baseline_data.TVOC_base, HEX);
#endif
      sensor->setIAQBaseline(baseline_data.eCO2_base, baseline_data.TVOC_base);
#ifdef DEBUG
    } else {
      dump_sgp30_serial("Our serial != baseline #", baseline_data.serialnumber,
                        "");
#endif
    }
  }
}

void zero_sgp30_1w(DS2438 *device) {
  // Zero out, but according to what we think 'zero' is ;)
  device->setTemperature((int8_t)0);
  device->setVDDVoltage((uint16_t)0);
  device->setVADVoltage((uint16_t)0);
  device->setCurrent((int16_t)-1023);
}

boolean baseline_sgp30(Adafruit_SGP30 *sensor) {
  uint16_t eCO2_base, TVOC_base;
  if (!sensor->getIAQBaseline(&eCO2_base, &TVOC_base)) {
    error_flash(500);
#ifdef DEBUG
    Serial.println("getIAQBaseline failed");
#endif
    return false;  // Didn't work
  }

#ifdef DEBUG
  Serial.print("Baseline values: eCO2: 0x");
  Serial.print(eCO2_base, HEX);
  Serial.print(" & TVOC: 0x");
  Serial.println(TVOC_base, HEX);
#endif

  // Copy serial number to our baseline array
  for (int lp = 0; lp < SGP30_SERIAL_LENGTH; lp++) {
    baseline_data.serialnumber[lp] = sensor->serialnumber[lp];
  }

  // And the actual baseline data
  baseline_data.eCO2_base = eCO2_base;
  baseline_data.TVOC_base = TVOC_base;

  // And stash in EEPROM

  int address = BASELINE_ADDRESS;
  // Indicator the following is data is good
  EEPROM.update(address++, BASELINE_MARKER);

  EEPROM.put(address, baseline_data);

  return true;  // Everything good
}
