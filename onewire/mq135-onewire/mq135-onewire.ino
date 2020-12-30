#include <DS18B20.h>

// To turn on DEBUG, define it in the common header:
#include <onewire-common.h>

// 1-Wire pin
#define PIN_ONE_WIRE 11

// How often to take readings (in ms)
#define READ_INTERVAL 1000

// Keep a moving average over this many readings
#define MA_READINGS 48

// Pin for analog input from MQ135
#define PIN_A_MQ135 A0  

// Init Hub
OneWireHub hub = OneWireHub(PIN_ONE_WIRE);
DS18B20 *ds18b20;

// Init delay, MQ135 needs some time to heat up, suggest to use delay 3 minutes => 3 * 60 * 1000
#ifdef DEBUG
  #define INIT_DELAY 5000
#else
  #define INIT_DELAY 180000
#endif

// Vars to calculate MA. Everything in integers to avoid rounding errors.
uint32_t ma_total = 0;
uint32_t ma_output = 0;
boolean ma_init = false;

void setup() {
  #ifdef DEBUG    
    Serial.begin(115200);
    Serial.println("OneWire-Hub MQ135 sensor");
  #endif

  error_flash(1000);

  #ifdef INIT_DELAY
    #ifdef DEBUG    
      Serial.print("Init delay...");
    #endif
    unsigned long delayEnd= millis() + INIT_DELAY;
    while (delayEnd > millis()) {
      digitalWrite(LED_BUILTIN, ((millis() % 100) < 50) ? 1 : 0);
    }
    #ifdef DEBUG    
      Serial.println("done");
    #endif
  #endif

  // 1W address
  uint8_t addr_1w[7];
  get_address(addr_1w);

  ds18b20 = new DS18B20(DS18B20::family_code, addr_1w[1], addr_1w[2], addr_1w[3], addr_1w[4], addr_1w[5], addr_1w[6]);
  #ifdef DEBUG
    dumpAddress("1-Wire DS18B20 address: ", ds18b20, "");
  #endif
  ds18b20->setTemperatureRaw(0);
  hub.attach(*ds18b20);
}

// Main loop

unsigned long next_reading = 0;
void loop() {
  // Handle 1-Wire traffic
  hub.poll();

  // Return if it's not time for the next reading yet
  if (millis() < next_reading) return;

  // Time to get readings
  next_reading = millis() + READ_INTERVAL;

  uint16_t mq135 = analogRead(PIN_A_MQ135);
  // ADC is 10 bit, but DS18B20 temperature is 12 bit, so shift left to allow for extra precision.
  #ifdef DEBUG
      Serial.print("Raw: "); Serial.print(mq135);
  #endif
  // << 2 doesn't work - huh?! :(
  mq135 *= 4;
  #ifdef DEBUG
      Serial.print(" Shift: "); Serial.print(mq135);
  #endif

  // Modify moving average accordingly
  if (!ma_init) {
    // First reading, just use the one we read
    ma_output = mq135;
    ma_total = ma_output * MA_READINGS;
    ma_init = true;
  } else {
    // Subtract one reading from total to give (MA_READINGS - 1) values totalled up.
    ma_total -= ma_output;
    // Add the value we just read
    ma_total += mq135;
    // And divide the total by readings to get the current average
    ma_output = ma_total / MA_READINGS;
  }

  // The ADC is 10 bit (0 -> 1023), but DS18B20 has 12 bit resolution (-55 -> +125)
  // so scale accordingly before doing anything else.

  float output = ma_output;
  output *= 180;
  // Divide by 12 bits due to shift above giving extra precision for MA
  output /= 4096;
  output -= 55;

  #ifdef DEBUG
    Serial.print(" MA: "); Serial.print(ma_output);
    Serial.print(" Output temp: "); Serial.println(output);
  #endif

  ds18b20->setTemperature(output);
}

