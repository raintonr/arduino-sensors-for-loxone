#include <EEPROM.h>
#include "OneWireHub.h"
#include "DS18B20.h"
#include <TrueRandom.h>
#include "Adafruit_SHT31.h"

//#define DEBUG

// 1-Wire pin
#define PIN_ONE_WIRE 11

// How often to take readings (in ms)
#define READ_INTERVAL 3000

// How long do we calculate delta over?
#define DELTA_INTERVAL 60000
// So how many readings do we need to store?
// Few more for good measure
#define DELTA_COUNT 20

// And rolling buffer
struct Reading {
  unsigned long timestamp;
  float temperature;
  float humidity; 
};
struct Reading deltas[DELTA_COUNT];

// Init SHT31
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// Init Hub
OneWireHub hub = OneWireHub(PIN_ONE_WIRE);
// We will use 4 sensors:
// - Temp
// - Temp delta
// - Humidity
// - Humidity delta
DS18B20 *ds_tmp, *ds_dtmp, *ds_hum, *ds_dhum;

// Starting conditions
boolean first = true;
int current_slot = 0;

void zero1w(DS18B20 *device) {
  device->setTemperatureRaw((int16_t) 0);
}

void error_flash(int ms) {
  unsigned long stopFlash = millis() + ms;
  while (stopFlash > millis()) {
    digitalWrite(LED_BUILTIN, (millis() % 40 < 10) ? 1 : 0);
  }
  digitalWrite(LED_BUILTIN, 0);
}

void setup() {
  #ifdef DEBUG    
    Serial.begin(115200);
    Serial.println("OneWire-Hub SHT31 sensor");
  #endif

  error_flash(1000);

  if (!sht31.begin(0x44)) {   // Set to 0x45 for alternate i2c addr
    #ifdef DEBUG    
      Serial.println("Couldn't find SHT31");
    #endif
    // Turn the LED on and loop forever
    digitalWrite(LED_BUILTIN, 1);
    while (1) delay(1);
  }

  // 1W address
  // We will actually provide multiple devices on sequential addresses
  uint8_t addr_1w[7];
  
  if (EEPROM.read(0) == '#') {
    // 1W address storred in EEPROM
    #ifdef DEBUG
      Serial.println("Reading 1W address from EEPROM");
    #endif
    for (int lp = 0; lp < 7; lp++) {
        addr_1w[lp] = EEPROM.read(lp + 1);
    }
  } else {
    // 1W address not set, generate random value
    #ifdef DEBUG
      Serial.println("Generating random 1W address");
    #endif
    for (int lp = 0; lp < 7; lp++) {
      // Make sure last byte can be incremented for sequential addresses we need
        addr_1w[lp] = (lp == 0) ? DS18B20::family_code : ((lp != 6) ? TrueRandom.random(256): TrueRandom.random(252));
        EEPROM.write(lp + 1, addr_1w[lp]);
    }
    EEPROM.write(0, '#');
  }

  // Init 4 required sensors
  ds_tmp  = new DS18B20(addr_1w[0], addr_1w[1], addr_1w[2], addr_1w[3], addr_1w[4], addr_1w[5], addr_1w[6]);
  ds_dtmp = new DS18B20(addr_1w[0], addr_1w[1], addr_1w[2], addr_1w[3], addr_1w[4], addr_1w[5], addr_1w[6] + 1);
  ds_hum  = new DS18B20(addr_1w[0], addr_1w[1], addr_1w[2], addr_1w[3], addr_1w[4], addr_1w[5], addr_1w[6] + 2);
  ds_dhum = new DS18B20(addr_1w[0], addr_1w[1], addr_1w[2], addr_1w[3], addr_1w[4], addr_1w[5], addr_1w[6] + 3);
  zero1w(ds_tmp);
  zero1w(ds_dtmp);
  zero1w(ds_hum);
  zero1w(ds_dhum);
  hub.attach(*ds_tmp);
  hub.attach(*ds_dtmp);
  hub.attach(*ds_hum);
  hub.attach(*ds_dhum);
  #ifdef DEBUG
    dumpAddress("1-Wire ds_tmp address:  ", ds_tmp, "");
    dumpAddress("1-Wire ds_dtmp address: ", ds_dtmp, "");
    dumpAddress("1-Wire ds_hum address:  ", ds_hum, "");
    dumpAddress("1-Wire ds_dhum address: ", ds_dhum, "");
  #endif
}

// Function to return number of next/prev slots (just inc/dec but account for wrap).
int next_slot(int slot) {
  return (++slot) % DELTA_COUNT;
}
int prev_slot(int slot) {
  return (slot > 0) ? (slot - 1) : (DELTA_COUNT - 1);
}

// Function to place current reading in slot and work out delta

int find_interval_reading(unsigned long old_stamp) {
  // TODO
  return next_slot(current_slot);
  
  #ifdef DEBUG
    Serial.print("Looking for reading before "); Serial.println(old_stamp);
  #endif
  int found_slot = -1;
  int check_slot = prev_slot(current_slot);
  do {
    // To match, must be older than interval, but not too old to stop wrapping timestamps
    if (deltas[check_slot].timestamp < old_stamp && old_stamp - deltas[check_slot].timestamp < DELTA_INTERVAL * 2) {
      // Found it
      #ifdef DEBUG
        Serial.print("Found it at "); Serial.println(deltas[check_slot].timestamp );
      #endif
      found_slot = check_slot;
    } else {
      check_slot = prev_slot(check_slot);
    }
  } while (check_slot != current_slot && found_slot < 0);
  return found_slot;
}

float delta_calc(float current_value, float old_value) {
  float absolute_delta = current_value - old_value;
  float pct_delta = absolute_delta / (old_value / 100);

  #ifdef DEBUG
    Serial.print("Old: "); Serial.print(old_value);
    Serial.print(" New: "); Serial.print(current_value);
    Serial.print(" Abs: "); Serial.print(absolute_delta);
    Serial.print(" Pct: "); Serial.println(pct_delta);
  #endif

  // Library makes sure values fall within -55 -> +125 limit so don't worry
  return pct_delta;
}

void loop() {
  #ifdef DEBUG
    Serial.print("Slot: "); Serial.println(current_slot);
  #endif

  unsigned long this_stamp = millis();
  
  float temperature = sht31.readTemperature();
  float humidity = sht31.readHumidity();
  if (isnan(temperature) || isnan(humidity)) {
    // Something bad
    error_flash(2000);
    #ifdef DEBUG
      Serial.println("Failed to read sensor");
    #endif
  } else {
    if (first) {
      // Fill up the delta buffer with initial values
      for (int lp = 0; lp < DELTA_COUNT; lp++) {
        deltas[lp].timestamp = this_stamp;
        deltas[lp].temperature = temperature;
        deltas[lp].humidity = humidity;
      }
      // We're not on the first reading any more
      first = false;
    } else {
      // Find a value old enough to create delta from
      int period_slot = find_interval_reading(this_stamp - DELTA_INTERVAL);

      if (period_slot < 0) {
        // No slot old enough found
        #ifdef DEBUG
          Serial.println("No reading old enough for interval");
        #endif
        // Just testing...
        ds_dtmp->setTemperature(0.1f);
        ds_dhum->setTemperature(0.2f);
      } else {
        #ifdef DEBUG
          Serial.print("Using old values from slot "); Serial.println(period_slot);
        #endif
        ds_dtmp->setTemperature(0.3f);
        ds_dhum->setTemperature(0.4f);
//        float delta;
//        delta = delta_calc(temperature, deltas[period_slot].temperature);
//        ds_dtmp->setTemperature(delta);
//        delta = delta_calc(humidity, deltas[period_slot].humidity);
//        ds_dhum->setTemperature(delta);
      }
    }
    // Always set current values
    deltas[current_slot].timestamp = this_stamp;
    deltas[current_slot].temperature = temperature;
    deltas[current_slot].humidity = humidity;
    // Move to the next slot for deltas
    current_slot = next_slot(current_slot);

    // Temperature is native
    ds_tmp->setTemperature(temperature);

    // Scale humidity 0 -> 100 to fit in between -55 -> +125
    humidity *= 1.8;
    humidity -= 55;
    #ifdef DEBUG
      Serial.print("Scaled humidity is "); Serial.println(humidity);
    #endif
    ds_hum->setTemperature(humidity);
  }

  // Wait for re-read and poll 1-wire during this time
  unsigned long stopPolling = this_stamp + READ_INTERVAL;
  while (stopPolling > millis()) {
    digitalWrite(LED_BUILTIN, (millis() % 200 < 50) ? 1 : 0);
    #ifdef DEBUG
      // Pretend to be polled every 1s
      delay(1000);
    #endif
    hub.poll();
  }
}

// Prints the device address to console
#ifdef DEBUG
void dumpAddress(const char *prefix, const OneWireItem *item, const char *postfix) {
  Serial.print(prefix);
  
  for (int lp = 0; lp < 8; lp++) {
      Serial.print(item->ID[lp] < 16 ? "0":"");
      Serial.print(item->ID[lp], HEX);
      Serial.print((lp < 7) ? "." : "");
  }

  Serial.println(postfix);
}
#endif
