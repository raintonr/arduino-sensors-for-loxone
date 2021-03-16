// WORK IN PROGRESS!

// A lot of this shamelessly copied from Adafruit examples... ahem... ;)

#include <Arduino.h>
#include <Wire.h>
#include "Adafruit_SHT31.h"
#include "Adafruit_SGP30.h"

// We're going to calculate an Indoor Air Quality (IAQ) index from 0-500.
//
// German Federal Environmental Agency has a translation from TVOC (in ppb) to quality as follows:
// TVOC (ppb) -> Quality level -> Maps to our IAQ
// 0-65          Excellent        0-100
// 65-220        Good             100-200
// 220-660       Moderate         200-300
// 660-2200      Poor             300-400
// 2200+         Unhealthy        400-500
//
// Source: https://help.atmotube.com/faq/5-iaq-standards/
// 
// CO2 (ppm)  -> Quality level -> Maps to our IAQ
// 250-350       Excellent        0-100
// 350-1000      Good             100-200
// 1000-2000     Moderate         200-300
// 2000-5000     Poor             300-400
// 5000+         Unhealthy        400-500
//
// Source: https://ohsonline.com/Articles/2016/04/01/Carbon-Dioxide-Detection-and-Indoor-Air-Quality-Control.aspx?m=1&Page=2
//
// SGP30 returns:
// - TVOC: 0-60000ppb
// - CO2: 400-60000ppm
//
// It's pointless returning very high values because those are 'off the scale' bad.
// Looking at the above tables, within the outputs of a DS2438 a reasonable solution for translation would be:
// CO2     -> Temperature (13 bit)
// TVOC    -> Current (11 bit)
// Our IAQ -> VDD Voltage (10 bit)
//
// Use a functional approximation calculator to convert the above tables into approximation
// for our IAQ. This yields:
// - TVOC IAQ = -277.9028+89.268*ln(x)
// - CO2 IAQ = -578.0919+114.6791*ln(x)
//

/* return absolute humidity [mg/m^3] with approximation formula
* @param temperature [°C]
* @param humidity [%RH]
*/
uint32_t getAbsoluteHumidity(float temperature, float humidity) {
    // approximation formula from Sensirion SGP30 Driver Integration chapter 3.15
    const float absoluteHumidity = 216.7f * ((humidity / 100.0f) * 6.112f * exp((17.62f * temperature) / (243.12f + temperature)) / (273.15f + temperature)); // [g/m^3]
    const uint32_t absoluteHumidityScaled = static_cast<uint32_t>(1000.0f * absoluteHumidity); // [mg/m^3]
    return absoluteHumidityScaled;
}

Adafruit_SGP30 sgp;
Adafruit_SHT31 sht31 = Adafruit_SHT31();

void setup() {
  Serial.begin(115200);
  while (!Serial) { delay(10); } // Wait for serial console to open!

  Serial.println("SHT31 + SGP30 test");
  if (! sgp.begin()){
    Serial.println("SGP30 not found :(");
    while (1);
  }
  Serial.print("Found SGP30 serial #");
  Serial.print(sgp.serialnumber[0], HEX);
  Serial.print(sgp.serialnumber[1], HEX);
  Serial.println(sgp.serialnumber[2], HEX);

  // If you have a baseline measurement from before you can assign it to start, to 'self-calibrate'
  //sgp.setIAQBaseline(0x8E68, 0x8F41);  // Will vary for each sensor!

  if (! sht31.begin(0x44)) {   // Set to 0x45 for alternate i2c addr
    Serial.println("Couldn't find SHT31");
    while (1) delay(1);
  }
}

int counter = 0;
void loop() {
  float temperature = sht31.readTemperature();
  float humidity = sht31.readHumidity();

  if (! isnan(temperature)) {  // check if 'is not a number'
    Serial.print("Temp *C = "); Serial.print(temperature); Serial.print("\t\t");
  } else { 
    Serial.println("Failed to read temperature");
  }
  
  if (! isnan(humidity)) {  // check if 'is not a number'
    Serial.print("Hum. % = "); Serial.print(humidity); Serial.print("\t\t");
  } else { 
    Serial.println("Failed to read humidity");
  }

  sgp.setHumidity(getAbsoluteHumidity(temperature, humidity));

  if (! sgp.IAQmeasure()) {
    Serial.println("Measurement failed");
    return;
  }
  Serial.print("TVOC "); Serial.print(sgp.TVOC); Serial.print(" ppb\t");
  Serial.print("eCO2 "); Serial.print(sgp.eCO2); Serial.print(" ppm\t");

  if (! sgp.IAQmeasureRaw()) {
    Serial.println("Raw Measurement failed");
    return;
  }
  Serial.print("Raw H2 "); Serial.print(sgp.rawH2); Serial.print(" \t");
  Serial.print("Raw Ethanol "); Serial.print(sgp.rawEthanol);
  
  Serial.println("");
 
  counter++;
  if (counter == 30) {
    counter = 0;

    uint16_t TVOC_base, eCO2_base;
    if (! sgp.getIAQBaseline(&eCO2_base, &TVOC_base)) {
      Serial.println("Failed to get baseline readings");
      return;
    }
    Serial.print("****Baseline values: eCO2: 0x"); Serial.print(eCO2_base, HEX);
    Serial.print(" & TVOC: 0x"); Serial.println(TVOC_base, HEX);
  }

  delay(1000);
}