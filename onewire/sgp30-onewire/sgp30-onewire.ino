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

// How many readings to take moving average over.
// For a reading frequency in Loxone of 60s, 15 (45s worth) of readings sounds
// good.
#define MA_READINGS 15

// How often to record the SGP30 baseline (in ms)
#define BASELINE_INTERVAL 28800000  // 8 hours

// Init Hub
OneWireHub hub = OneWireHub(PIN_ONE_WIRE);

// We will present 2 x sensors:
// - SGP30 CO2, TVOC & IAQ index
// - SHT31 temperature & humidity
DS2438 *ds2438_sgp30, *ds2438_sht31;

// Shamelessly copied from Adafruit examples... ahem... ;)
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

// Moving average instances for readings to output
template <typename MA_Type, size_t MA_Count>
class MovingAverageCalculator {
 public:
  // Process the given input and return the averaged output.
  // If we try and keep a running total of floats over time this will eventually
  // become inaccurate because of rounding so just do things brute force & store
  // all the inputs then calculate the average... it's not like the CPU has
  // anything better to do anyhow ;)
  MA_Type operator()(MA_Type input) {
    if (!init) {
      for (int lp = 0; lp < MA_Count; lp++) {
        ma_readings[lp] = input;
      }
      init = true;
    } else {
      if (++next_reading >= MA_Count) next_reading = 0;
      ma_readings[next_reading] = input;
    }

    MA_Type output = 0;
    for (int lp = 0; lp < MA_Count; lp++) {
      output += ma_readings[lp];
    }
    return output / MA_Count;
  }

 private:
  MA_Type ma_readings[MA_Count];
  int next_reading = 0;
  boolean init = false;
};

// Float averages just use float for their calculations
MovingAverageCalculator<float, MA_READINGS> MA_temperature, MA_humidity;
// The following are uint_16_t so use a uint32_t for calculations
MovingAverageCalculator<uint32_t, MA_READINGS> MA_CO2, MA_TVOC;

// One time setup function

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("OneWire-Hub SHT31 + SGP30 sensor");
#endif

  error_flash(1000);

  init_sgp30(&sgp30);

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

// Class to perform Logarithmic regression

class LogarithmicRegressionCalculator {
  float add, mult;

 public:
  LogarithmicRegressionCalculator(float add_init, float mult_init) {
    add = add_init;
    mult = mult_init;
  }

 public:
  float calc(float x) {
    float y = 0;
    if (x > 0) {
      y = log(x);
      y *= mult;
      y += add;
      if (y < 0) y = 0;
    }
    return y;
  }
};

// To calculate an Indoor Air Quality (IAQ) index from 0-500...
//
//      TVOC (ppb): 100 333 1000 3333 8332
// Maps to our IAQ: 10 100 200 300 400
//
//       CO2 (ppm): 400 600 1000 1500 2500
// Maps to our IAQ: 10 100 200 300 400
//
// This is basically the bands from Awair:
//
// https://support.getawair.com/hc/en-us/articles/360039242373-Air-Quality-Factors-Measured-By-Awair-Element
//
// Use a functional approximation calculator to convert the above tables
// into approximation for our IAQ. This yields:
// - TVOC IAQ = -402.3294+87.6842*ln(x)
// - CO2 IAQ = -1269.4589+213.6673*ln(x)
//

LogarithmicRegressionCalculator TVOC_LR(-402.3294, 87.6842);
LogarithmicRegressionCalculator CO2_LR(-1269.4589, 213.6673);

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

  // Adjustment SGP30 for current humidity
  sgp30.setHumidity(getAbsoluteHumidity(temperature, humidity));

  // Smooth readings for output
  temperature = MA_temperature(temperature);
  humidity = MA_humidity(humidity);

#ifdef DEBUG
  Serial.print("Temp: ");
  Serial.print(temperature);
  Serial.print("\tHum: ");
  Serial.print(humidity);
#endif

  // Stash temperature & humidity in it's sensor
  ds2438_sht31->setTemperature(temperature);
  set_sht31_humidity(ds2438_sht31, humidity);

  // Measure SGP30... in a loop until we get something good.
  while (!sgp30.IAQmeasure()) {
    // Something bad - flash LED
    error_flash(READ_INTERVAL - 100, 100);
#ifdef DEBUG
    Serial.println("IAQmeasure failed");
#endif
    // Re-initialise before next attempt
    init_sgp30(&sgp30);
  }

  // Smooth readings
  uint16_t eCO2 = MA_CO2(sgp30.eCO2);
  uint16_t TVOC = MA_TVOC(sgp30.TVOC);

#ifdef DEBUG
  Serial.print("\teCO2: ");
  Serial.print(sgp30.eCO2);
  Serial.print("\tMA: ");
  Serial.print(eCO2);
  Serial.print("\tTVOC: ");
  Serial.print(sgp30.TVOC);
  Serial.print("\tMA: ");
  Serial.print(TVOC);
#endif

  // SGP30 returns:
  // - TVOC: 0-60000ppb
  // - CO2: 400-60000ppm
  //
  // It's pointless returning very high values because those are 'off the
  // scale' bad. Within the outputs of a DS2438 a reasonable solution for
  // translation would be:
  // - CO2       -> Current (11 bit)
  // - TVOC      -> Temperature (13 bit)
  // - IAQ index -> VDD Voltage (10 bit)

  // Calculate IAQ indexes and use the highest one.
  // Do this before messing with scale, etc. of the values when they are stored.
  float IAQ_TVOC = TVOC_LR.calc(TVOC);
  float IAQ_CO2 = CO2_LR.calc(eCO2);

#ifdef DEBUG
  Serial.print("\tIAQ TVOC: ");
  Serial.print(IAQ_TVOC);
  Serial.print("\tIAQ CO2: ");
  Serial.print(IAQ_CO2);
#endif

  // Put the highest IAQ reading in VDD Voltage (0-1023) so in Loxone we get:
  // 0 -> 0
  // 10.23 -> 1023
  uint16_t IAQ_index = IAQ_TVOC > IAQ_CO2 ? IAQ_TVOC : IAQ_CO2;
  ds2438_sgp30->setVDDVoltage(IAQ_index);

  // CO2 goes into sensor's current (11 bits) but as minimal reading is 400,
  // offset by that so in Loxone we get:
  // -0.25 -> 400
  // 0.25 -> 2447
  eCO2 -= 1423;  // 400 + 1023
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
  float fTVOC = TVOC;
  fTVOC /= 45.51;
  fTVOC -= 55;
#ifdef DEBUG
  Serial.print("\tDS2438 TVOC: ");
  Serial.print(fTVOC);
#endif
  ds2438_sgp30->setTemperature(fTVOC);

#ifdef DEBUG
  unsigned long seconds_to_baseline = next_baseline;
  seconds_to_baseline -= millis();
  seconds_to_baseline /= 1000;
  Serial.print("\tNext baseline in: ");
  Serial.println(seconds_to_baseline);
#endif

  // Save baseline if it's time
  if (millis() > next_baseline &&  // Yes it's time
      baseline_sgp30(&sgp30)       // Yes, save worked
  ) {
    // Saved baseline - set timestamp for next one
    next_baseline = this_stamp + BASELINE_INTERVAL;
  }
}
