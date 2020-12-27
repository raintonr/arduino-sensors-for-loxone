#include <EEPROM.h>
#include "OneWireHub.h"
#include "DS2438New.h"
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
#define DELTA_COUNT 21

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
// We use two sensors
DS2438New *t_ds2438, *h_ds2438;

// 1W address
// We will actually privide 2 devices on sequential addresses
uint8_t addr_1w[7] = { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00 };

// Starting conditions
boolean first = true;
int current_slot = 0;

void zero1w(DS2438New *device) {
  device->setCurrent(0);
  device->setVDDVoltage(0);
  device->setVADVoltage(0);
  device->setTemperature((int8_t) 0);
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

  if (EEPROM.read(0) == '#') {
    // 1W address storred in EEPROM
    #ifdef DEBUG
      Serial.println("Reading 1W address from EEPROM");
    #endif
    for (int lp = 1; lp < 7; lp++) {
        addr_1w[lp] = EEPROM.read(lp);
    }
  } else {
    // 1W address not set, generate random value
    #ifdef DEBUG
      Serial.println("Generating random 1W address");
    #endif
    for (int lp = 1; lp < 7; lp++) {
      // Make sure last byte can be incremented for sequential addresses we need
        addr_1w[lp] = (lp != 6) ? TrueRandom.random(256): TrueRandom.random(255);
        EEPROM.write(lp, addr_1w[lp]);
    }
    EEPROM.write(0, '#');
  }

  // Init 2 sensors for temperature & humidity

  t_ds2438 = new DS2438New(DS2438New::family_code, addr_1w[1], addr_1w[2], addr_1w[3], addr_1w[4], addr_1w[5], addr_1w[6]);
  zero1w(t_ds2438);
  hub.attach(*t_ds2438);
  #ifdef DEBUG
    dumpAddress("1-Wire DS2438 for temperature address: ", t_ds2438, "");
  #endif

//  h_ds2438 = new DS2438New(DS2438New::family_code, addr_1w[1], addr_1w[2], addr_1w[3], addr_1w[4], addr_1w[5], addr_1w[6] + 1);
//  zero1w(h_ds2438);
//  hub.attach(*h_ds2438);
//  dumpAddress("1-Wire DS2438 for humidity address: ", h_ds2438, "");
}

// Function to return number of next/prev slots (just inc/dec but account for wrap).
int next_slot(int slot) {
  return (slot + 1) % DELTA_COUNT;
}
int prev_slot(int slot) {
  slot--;
  if (slot < 0) {
    slot = DELTA_COUNT - 1;
  }
  return slot;
}

// Function to place current reading in slot and work out delta

int find_interval_reading(unsigned long old_stamp) {
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

uint16_t delta_calc(float current_value, float old_value) {
  uint16_t delta = 0;
  float absolute_delta = current_value - old_value;
  float pct_delta = absolute_delta / (old_value / 100);

  #ifdef DEBUG
    Serial.print("Old: "); Serial.print(old_value);
    Serial.print(" New: "); Serial.print(current_value);
    Serial.print(" Abs: "); Serial.print(absolute_delta);
    Serial.print(" Pct: "); Serial.print(pct_delta);
  #endif

  // Delta percent needs to be 0 -> 1023 so scale 10.23x
  // 0 -> 1023 becomes -50% -> +50%
  pct_delta *= 10.23;
  pct_delta += 512;
  if (pct_delta < 0) {
    pct_delta = 0;
  } else if (pct_delta > 1023) {
    pct_delta = 1023;
  }
  // And cast to final return value
  delta = pct_delta;
  #ifdef DEBUG
    Serial.print(" Int: "); Serial.println(delta);
  #endif

  return delta;
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
    if (0 && first) {
      // Fill up the delta buffer with initial values
      for (int lp = 0; lp < DELTA_COUNT; lp++) {
        deltas[lp].timestamp = this_stamp;
        deltas[lp].temperature = temperature;
        deltas[lp].humidity = humidity;
      }
      // We're not on the first reading any more
      first = false;
    } else if (0) {
      // Find a value old enough to create delta from
      int period_slot = find_interval_reading(this_stamp - DELTA_INTERVAL);

      if (period_slot < 0) {
        // No slot old enough found
        #ifdef DEBUG
          Serial.println("No reading old enough for interval");
        #endif
      } else {
        #ifdef DEBUG
          Serial.print("Using old values from slot "); Serial.println(period_slot);
        #endif
        uint16_t delta = delta_calc(temperature, deltas[period_slot].temperature);
        t_ds2438->setVDDVoltage(delta);
        delta = delta_calc(humidity, deltas[period_slot].humidity);
        t_ds2438->setVADVoltage(delta);
      }
    }
    // Always set current values
    if (0) {
      deltas[current_slot].timestamp = this_stamp;
      deltas[current_slot].temperature = temperature;
      deltas[current_slot].humidity = humidity;
    }
    // Move to the next slot for deltas
    current_slot++; // = next_slot(current_slot);

    t_ds2438->setTemperature(temperature);

    // Testing VAD & VDD
    uint16_t delta;
    delta = current_slot + 1;
    t_ds2438->setVDDVoltage(delta);
    delta *= 10;
    t_ds2438->setVADVoltage(delta);
    
    // Have to scale humidity to fit in current (between -1023 & +1023)
    // So divide multiply by 20.46 and subtract 1023
    // This ends up in Loxone between -0.25 & +0.25
    humidity *= 20.46;
    humidity -= 1023;
    int16_t humidity_raw = humidity;
    #ifdef DEBUG
      Serial.print("Raw humidity is "); Serial.println(humidity_raw);
    #endif
    t_ds2438->setCurrent(humidity_raw);

  }

  // Wait for re-read and poll 1-wire during this time
  unsigned long stopPolling = millis() + READ_INTERVAL;
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
  
  for (int i = 0; i < 8; i++) {
      Serial.print(item->ID[i] < 16 ? "0":"");
      Serial.print(item->ID[i], HEX);
      Serial.print((i < 7)?".":"");
  }

  Serial.println(postfix);
}
#endif
