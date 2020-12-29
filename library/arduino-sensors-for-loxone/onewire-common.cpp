#include <EEPROM.h>
#include <TrueRandom.h>
#include "onewire-common.h"

void get_address(uint8_t *addr_1w) {
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
        addr_1w[lp] = TrueRandom.random(256);
        EEPROM.write(lp, addr_1w[lp]);
    }
    EEPROM.write(0, '#');
  }
}

// Flash our LED to indicate an error

void error_flash(int ms) {
  unsigned long stopFlash = millis() + ms;
  while (stopFlash > millis()) {
    digitalWrite(LED_BUILTIN, (millis() % 40 < 10) ? 1 : 0);
  }
  digitalWrite(LED_BUILTIN, 0);
}

// Prints the device address to console
#ifdef DEBUG
void dumpAddress(const char *prefix, const OneWireItem *item, const char *postfix) {
    Serial.print(prefix);

    for (int lp = 0; lp < 8; lp++) {
        Serial.print(item->ID[lp] < 16 ? "0":"");
        Serial.print(item->ID[lp], HEX);
        Serial.print((lp < 7)?".":"");
    }

    Serial.println(postfix);
}
#endif