#include <Adafruit_SHT31.h>
#include <DS2438.h>

// To turn on DEBUG, define it in the common header:
#include <asfl-common.h>
#include <onewire-common.h>
#include <sht31-common.h>

// 1-Wire pin
#define PIN_ONE_WIRE 11

// How often to take readings (in ms)
#define READ_INTERVAL 3000

// How long do we calculate delta over?
#define DELTA_INTERVAL 60000

// So how many readings do we need to store?
// Should be DELTA_INTERVAL / DELTA_INTERVAL + 1
#define DELTA_COUNT 21

// How many degrees/percentage humidity points will equate to +/- full range for
// delta values Ie. a value of 10 here indicates full range in the delta values
// signify -5 to +5
#define TEMPERATURE_DELTA_RANGE 20
#define HUMIDITY_DELTA_RANGE 100

// And rolling buffer
struct Reading {
  unsigned long timestamp;
  float temperature;
  float humidity;
};
struct Reading deltas[DELTA_COUNT];

// OneWire
OneWireHub hub = OneWireHub(PIN_ONE_WIRE);
DS2438 *ds2438;

// Starting conditions
boolean first = true;
int8_t current_slot = 0;

void setup() {
#ifdef DEBUG
  Serial.begin(115200);
  Serial.println("OneWire-Hub SHT31 sensor");
#endif

  error_flash(1000);

  init_sht31();

  // 1W address
  uint8_t addr_1w[7];
  get_address(addr_1w);

  ds2438 = new DS2438(DS2438::family_code, addr_1w[1], addr_1w[2], addr_1w[3],
                      addr_1w[4], addr_1w[5], addr_1w[6]);
#ifdef DEBUG
  dumpAddress("1-Wire DS2438 address: ", ds2438, "");
#endif
  zero_sht31_1w(ds2438);
  hub.attach(*ds2438);
}

// Function to return number of next/prev slots (just inc/dec but account for
// wrap).
int8_t next_slot(int8_t slot) { return (++slot) % DELTA_COUNT; }
int8_t prev_slot(int8_t slot) {
  return (slot > 0) ? (slot - 1) : (DELTA_COUNT - 1);
}

// You'd think that one could just use the oldest slot but... timing of
// hub.poll isn't guaranteed to be 'quick' so it's possible that it could delay
// things and that we should use one of the previous slots.
//
// Yes, this is a little convoluted/complicated but really - what else is this
// CPU gonna be doing? ;)

int8_t find_interval_reading(unsigned long old_stamp) {
#ifdef DEBUG
  Serial.print("Looking for reading before ");
  Serial.println(old_stamp);
#endif
  int8_t found_slot = -1;
  int8_t check_slot = prev_slot(current_slot);
  do {
    // To match, must be older than interval, but not too old to stop
    // wrapping timestamps
    if (deltas[check_slot].timestamp < old_stamp &&
        old_stamp - deltas[check_slot].timestamp < DELTA_INTERVAL * 2) {
// Found it
#ifdef DEBUG
      Serial.print("Found it at ");
      Serial.println(deltas[check_slot].timestamp);
#endif
      found_slot = check_slot;
    } else {
      check_slot = prev_slot(check_slot);
    }
  } while (check_slot != current_slot && found_slot < 0);
  return found_slot;
}

// Calculate out delta based on supplied range (so each reading can have
// different sensitivity).

uint16_t delta_calc(float current_value, float old_value, float range) {
  float abs_delta = current_value - old_value;

#ifdef DEBUG
  Serial.print("Old: ");
  Serial.print(old_value);
  Serial.print(" New: ");
  Serial.print(current_value);
  Serial.print(" Abs: ");
  Serial.print(abs_delta);
#endif

  // Delta needs to be 0 -> 1023
  // Depends on range: 0 -> 1023 becomes -range -> +range
  abs_delta *= 1024;
  abs_delta /= range;
  abs_delta += 512;

  // Cast to final return value (within limits)
  uint16_t delta = abs_delta < 0 ? 0 : abs_delta > 1023 ? 1023 : abs_delta;
#ifdef DEBUG
  Serial.print(" Int: ");
  Serial.println(delta);
#endif

  return delta;
}

// Main loop

unsigned long next_reading = 0;
void loop() {
  // Handle 1-Wire traffic
  hub.poll();

  // Return if it's not time for the next reading yet
  if (millis() < next_reading) return;

  // Time to get readings

  // Work out next time
  unsigned long this_stamp = millis();
  next_reading = this_stamp + READ_INTERVAL;

#ifdef DEBUG
  Serial.print("Slot: ");
  Serial.println(current_slot);
#endif

  float temperature, humidity;
  error_flash(100);
  read_sht31(&temperature, &humidity);

  if (first) {
    // Fill up the delta buffer with initial values
    for (int8_t lp = 0; lp < DELTA_COUNT; lp++) {
      deltas[lp].timestamp = this_stamp;
      deltas[lp].temperature = temperature;
      deltas[lp].humidity = humidity;
    }
    // We're not on the first reading any more
    first = false;
  } else {
    // Find a value old enough to create delta from
    int8_t period_slot = find_interval_reading(this_stamp - DELTA_INTERVAL);

    if (period_slot < 0) {
// No slot old enough found
#ifdef DEBUG
      Serial.println("No reading old enough for interval");
#endif
    } else {
#ifdef DEBUG
      Serial.print("Using old values from slot ");
      Serial.println(period_slot);
#endif
      ds2438->setVDDVoltage(delta_calc(temperature,
                                       deltas[period_slot].temperature,
                                       TEMPERATURE_DELTA_RANGE));
      ds2438->setVADVoltage(delta_calc(humidity, deltas[period_slot].humidity,
                                       HUMIDITY_DELTA_RANGE));
    }
  }

  // Always set current values
  deltas[current_slot].timestamp = this_stamp;
  deltas[current_slot].temperature = temperature;
  deltas[current_slot].humidity = humidity;
  // Move to the next slot for deltas
  current_slot = next_slot(current_slot);

  ds2438->setTemperature(temperature);
  set_sht31_humidity(ds2438, humidity);
#ifdef DEBUG
  Serial.println("");
#endif
}