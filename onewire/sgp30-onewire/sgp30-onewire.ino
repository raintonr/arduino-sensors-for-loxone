// WORK IN PROGRESS!

// A lot of this shamelessly copied from Adafruit examples... ahem... ;)

#include <Arduino.h>
#include <EEPROM.h>
#include <Wire.h>

// To turn on DEBUG, define it in the common header:
#include <onewire-common.h>
#include <sgp30-common.h>
#include <sht31-common.h>

Adafruit_SHT31 sht31;
Adafruit_SGP30 sgp30;

// 1-Wire pin
#define PIN_ONE_WIRE 11

// How often to take readings (in ms)
#define READ_INTERVAL 3000

// How often to record the SGP30 baseline (in ms)
#define BASELINE_INTERVAL 1000 * 60 * 60 * 4  // 4 hours

// Bytes 0-7 are reserved for 1 wire address so put define EEPROM address to
// store baseline after that
#define BASELINE_ADDRESS 16

// Init Hub
OneWireHub hub = OneWireHub(PIN_ONE_WIRE);

// We will present 2 x sensors:
// - SGP30 CO2, TVOC & IAQ index
// - SHT31 temperature & humidity
DS2438 *ds2438_sgp30, *ds2438_sht31;

/* return absolute humidity [mg/m^3] with approximation formula
 * @param temperature [°C]
 * @param humidity [%RH]
 */
uint32_t getAbsoluteHumidity(float temperature, float humidity) {
  // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
  const float absoluteHumidity =
      216.7f * ((humidity / 100.0f) * 6.112f *
                exp((17.62f * temperature) / (243.12f + temperature)) /
                (273.15f + temperature));  // [g/m^3]
  const uint32_t absoluteHumidityScaled =
      static_cast<uint32_t>(1000.0f * absoluteHumidity);  // [mg/m^3]
  return absoluteHumidityScaled;
}

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("OneWire-Hub SHT31 + SGP30 sensor");
#endif

  error_flash(1000);

  init_sgp30(&sgp30);
  int address = BASELINE_ADDRESS;
  if (EEPROM.read(address++) == '#') {
    // Looks like we have previously stored baseline available
    uint16_t eCO2_base, TVOC_base;

    eCO2_base = EEPROM.read(address + 1);
    eCO2_base << 8;
    eCO2_base += EEPROM.read(address);
    address += 2;

    TVOC_base = EEPROM.read(address + 1);
    TVOC_base << 8;
    TVOC_base += EEPROM.read(address);

#ifdef DEBUG
    Serial.print("Setting saved baseline values: eCO2: 0x");
    Serial.print(eCO2_base, HEX);
    Serial.print(" & TVOC: 0x");
    Serial.println(TVOC_base, HEX);
#endif

    sgp30.setIAQBaseline(eCO2_base, TVOC_base);
  }

  init_sht31(&sht31);

  // 1W address
  uint8_t addr_1w[7];
  get_address(addr_1w);

  ds2438_sgp30 = new DS2438(DS2438::family_code, addr_1w[1], addr_1w[2],
                            addr_1w[3], addr_1w[4], addr_1w[5], addr_1w[6]);
#ifdef DEBUG
  dumpAddress("1-Wire DS2438 address (SGP30): ", ds2438_sgp30, "");
#endif
  zero_sgp30_1w(ds2438_sgp30);
  hub.attach(*ds2438_sgp30);

  ds2438_sht31 = new DS2438(DS2438::family_code, addr_1w[1], addr_1w[2],
                            addr_1w[3], addr_1w[4], addr_1w[5], addr_1w[6] + 1);
#ifdef DEBUG
  dumpAddress("1-Wire DS2438 address (SHT31): ", ds2438_sht31, "");
#endif
  zero_sht31_1w(ds2438_sht31);
  hub.attach(*ds2438_sht31);
}

// Main loop

// Always read immediately
unsigned long next_reading = 0;
// Don't set a baseline until at least one interval has passed
unsigned long next_baseline = BASELINE_INTERVAL;

void loop() {
  // Handle 1-Wire traffic
  hub.poll();

  // Return if it's not time for the next reading yet
  if (millis() < next_reading) return;

  // Work out next time
  unsigned long this_stamp = millis();
  next_reading = this_stamp + READ_INTERVAL;

  // Read temperature & humidity for it's own sensor & SGP30 adjustment
  float temperature, humidity;
  read_sht31(&sht31, &temperature, &humidity);

#ifdef DEBUG
  Serial.print("Temp: ");
  Serial.print(temperature);
  Serial.print("\tHum: ");
  Serial.print(humidity);
#endif

  // Stash temperature & humidity in it's sensor
  ds2438_sht31->setTemperature(temperature);
  set_sht31_humidity(ds2438_sht31, humidity);

  // Adjustment SGP30 for current humidity
  sgp30.setHumidity(getAbsoluteHumidity(temperature, humidity));

  // And measure... in a loop until we get something good.
  while (!sgp30.IAQmeasure()) {
    // Something bad - flash LED
    error_flash(READ_INTERVAL - 100, 100);
#ifdef DEBUG
    Serial.println("IAQmeasure failed");
#endif
  }

#ifdef DEBUG
  Serial.print("\teCO2: ");
  Serial.print(sgp30.eCO2);
  Serial.print("\tTVOC: ");
  Serial.print(sgp30.TVOC);
#endif

  // SGP30 returns:
  // - TVOC: 0-60000ppb
  // - CO2: 400-60000ppm
  //
  // It's pointless returning very high values because those are 'off the scale'
  // bad. Within the outputs of a DS2438 a reasonable solution for translation
  // would be:
  // - CO2       -> Current (11 bit)
  // - TVOC      -> Temperature (13 bit)
  // - IAQ index -> VDD Voltage (10 bit)

  // CO2 goes into sensor's current (11 bits) but as minimal reading is 400,
  // offset by that So in Loxone we get: -0.25 -> 400 0.25 -> 2447
  int16_t eCO2 = sgp30.eCO2;
  eCO2 -= 400;
#ifdef DEBUG
  Serial.print("\tDS2438 eCO2: ");
  Serial.print(eCO2);
#endif
  ds2438_sgp30->setCurrent(eCO2);

  // Scale TVOC to fit in sensor's temperature (-55 to 125)
  // As temperature is 13 bit number that gives resolution of 0 to 8191
  // So divide TVOC by 45.51 then subtract 55 so in Loxone we get:
  // -55 -> 0
  // 125 -> 8192
  float TVOC = sgp30.TVOC;
  TVOC /= 45.51;
  TVOC -= 55;
#ifdef DEBUG
  Serial.print("\tDS2438 TVOC: ");
  Serial.print(TVOC);
#endif
  ds2438_sgp30->setTemperature(TVOC);

  // Calculate Indoor Air Quality (IAQ) index from 0-500.
  //
  // German Federal Environmental Agency has a translation from TVOC (in ppb) to
  // quality as follows:
  //
  // TVOC (ppb) -> Quality level -> Maps to our IAQ
  // 0-65        Excellent        0-100
  // 65-220      Good             100-200
  // 220-660     Moderate         200-300
  // 660-2200    Poor             300-400
  // 2200+       Unhealthy        400-500
  //
  // Source: https://help.atmotube.com/faq/5-iaq-standards/
  //
  // CO2 (ppm)  -> Quality level -> Maps to our IAQ
  // 350-450       Fresh air        0-100
  // 450-1000      Normal level     100-200
  // 1000-2500     Drowsiness       200-300
  // 2500-5000     Negative impact  300-400
  // 5000+         Unhealthy        400-500
  //
  // Source: https://axiomet.eu/gb/en/page/1954/air-quality-monitoring-indoors/
  //
  // Also see
  // https://support.getawair.com/hc/en-us/articles/360039242373-Air-Quality-Factors-Measured-By-Awair-Element
  //
  // Use a functional approximation calculator to convert the above tables into
  // approximation for our IAQ. This yields:
  // - TVOC IAQ = -277.9028+89.268*ln(x)
  // - CO2 IAQ = -649.3209+122.5306*ln(x)
  //

  float IAQ_TVOC = sgp30.TVOC;
  if (IAQ_TVOC > 0) {
    IAQ_TVOC = log(IAQ_TVOC);
    IAQ_TVOC *= 89.268;
    IAQ_TVOC -= 277.9028;
    if (IAQ_TVOC < 0) IAQ_TVOC = 0;
  }

  float IAQ_CO2 = sgp30.eCO2;
  if (IAQ_CO2 > 0) {
    IAQ_CO2 = log(IAQ_CO2);
    IAQ_CO2 *= 122.5306;
    IAQ_CO2 -= 649.3209;
    if (IAQ_CO2 < 0) IAQ_CO2 = 0;
  }

#ifdef DEBUG
  Serial.print("\tIAQ TVOC: ");
  Serial.print(IAQ_TVOC);
  Serial.print("\tIAQ CO2: ");
  Serial.println(IAQ_CO2);
#endif

  // Put the highest IAQ reading in VDD Voltage (0-1023) so in Loxone we get:
  // 0 -> 0
  // 10.23 -> 1023
  uint16_t IAQ_index = IAQ_TVOC > IAQ_CO2 ? IAQ_TVOC : IAQ_CO2;
  ds2438_sgp30->setVDDVoltage(IAQ_index);

  // Done for now if it's not time for to record the baseline
  if (millis() < next_baseline) return;

  uint16_t eCO2_base, TVOC_base;
  if (!sgp30.getIAQBaseline(&eCO2_base, &TVOC_base)) {
    // Something bad - flash LED
    error_flash(READ_INTERVAL - 100, 100);
#ifdef DEBUG
    Serial.println("getIAQBaseline failed");
#endif
    return;
  }

#ifdef DEBUG
  Serial.print("Baseline values: eCO2: 0x");
  Serial.print(eCO2_base, HEX);
  Serial.print(" & TVOC: 0x");
  Serial.println(TVOC_base, HEX);
#endif

  // OK, got a good baseline - set timestamp for next one
  next_baseline = this_stamp + BASELINE_INTERVAL;

  // And stash in EEPROM
  // Bytes 0-7 are reserved for 1 wire address so put these baselines in
  // address 16...
  int address = BASELINE_ADDRESS;
  EEPROM.update(address++, '#');  // Indicator the following is value is good

  EEPROM.update(address++, eCO2_base & 0xff);
  eCO2_base >> 8;
  EEPROM.update(address++, eCO2_base & 0xff);

  EEPROM.update(address++, TVOC_base & 0xff);
  TVOC_base >> 8;
  EEPROM.update(address++, TVOC_base & 0xff);
}
