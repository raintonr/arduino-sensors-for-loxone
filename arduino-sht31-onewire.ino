#include <EEPROM.h>
#include "OneWireHub.h"
#include "OneWireItem.h"
#include <TrueRandom.h>
#include "Adafruit_SHT31.h"

// 1-Wire pin
#define PIN_ONE_WIRE 11

// How often to take readings (in ms)
#define READ_INTERVAL 3000

// How many readings to calculate delta over
#define DELTA_INTERVAL 20

// And rolling buffer
float t_deltas[DELTA_INTERVAL];
float h_deltas[DELTA_INTERVAL];

// Init SHT31
Adafruit_SHT31 sht31 = Adafruit_SHT31();

// Init Hub
OneWireHub hub = OneWireHub(PIN_ONE_WIRE);
#include "DS2438New.h"
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
  device->setTemperature((int8_t) 0);
}

void setup() {
  Serial.begin(9600);

  if (!sht31.begin(0x44)) {   // Set to 0x45 for alternate i2c addr
    Serial.println("Couldn't find SHT31");
    while (1) delay(1);
  }

  if (EEPROM.read(0) == '#') {
    // 1W address storred in EEPROM
    Serial.println("Reading 1W address from EEPROM");
    for (int lp = 1; lp < 7; lp++) {
        addr_1w[lp] = EEPROM.read(lp);
    }
  } else {
    // 1W address not set, generate random value
    Serial.println("Generating random 1W address");
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

  h_ds2438 = new DS2438New(DS2438New::family_code, addr_1w[1], addr_1w[2], addr_1w[3], addr_1w[4], addr_1w[5], addr_1w[6] + 1);
  zero1w(h_ds2438);
  hub.attach(*h_ds2438);

  dumpAddress("1-Wire DS2438 for temperature address: ", t_ds2438, "");
  dumpAddress("1-Wire DS2438 for humidity address: ", h_ds2438, "");
}

// Function to return number of next slot (just add one but account for wrap).
int next_slot() {
  return (current_slot + 1) % DELTA_INTERVAL;
}

// Function to place current reading in slot and work out delta

uint16_t delta_calc(float current_value, float *deltas) {
  float delta = 0;
  if (first) {
    // No delta and just fill array with current value
    for (int lp = 0; lp < DELTA_INTERVAL; lp++) {
      deltas[lp] = current_value;
    }
  } else {
    delta = current_value - deltas[next_slot()];
    deltas[current_slot] = current_value;
  }
  return delta;
}

void loop() {
  float temp = sht31.readTemperature();
  Serial.print("Slot: "); Serial.print(current_slot);
  Serial.print("\t");
  
  if (! isnan(temp)) {  // check if 'is not a number'
    uint16_t d_temp = delta_calc(temp, t_deltas);

    t_ds2438->setTemperature(temp);
    t_ds2438->setVADVoltage(d_temp * 10);
    
    Serial.print("Temp *C = "); Serial.print(temp);
    Serial.print("\tdT *C = "); Serial.print(d_temp);
    Serial.print("\t");
  } else { 
    Serial.println("Failed to read temperature");
  }

  float humidity = sht31.readHumidity();
  if (! isnan(humidity)) {  // check if 'is not a number'
    uint16_t d_humidity = delta_calc(humidity, h_deltas);

    h_ds2438->setTemperature(humidity);
    h_ds2438->setVADVoltage(d_humidity * 10);
    
    Serial.print("Hum. % = "); Serial.print(humidity);
    Serial.print("\tdH % = "); Serial.println(d_humidity);
  } else { 
    Serial.println("Failed to read humidity");
  }

  // We're not on the first reading any more
  first = false;

  // Move to the next slot for deltas
  current_slot = next_slot();

  // Wait for re-read and poll 1-wire during this time
  unsigned long stopPolling = millis() + READ_INTERVAL;
  while (stopPolling > millis()) {
    hub.poll();
  }
}

// Prints the device address to console
void dumpAddress(const char *prefix, const OneWireItem *item, const char *postfix) {
  Serial.print(prefix);
  
  for (int i = 0; i < 8; i++) {
      Serial.print(item->ID[i] < 16 ? "0":"");
      Serial.print(item->ID[i], HEX);
      Serial.print((i < 7)?".":"");
  }

  Serial.println(postfix);
}
