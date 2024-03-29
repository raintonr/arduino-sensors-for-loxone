#include <EEPROM.h>
#include <TrueRandom.h>
#include <asfl-common.h>
#include <onewire-common.h>

void get_address(uint8_t *addr_1w) {
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
      // Don't allow the last byte of the address to be too high to account
      // for when more than one sensor is presented from a single device.
      addr_1w[lp] = TrueRandom.random(lp < 6 ? 256 : 256 - MAX_ONEWIRE_SENSORS);
      EEPROM.write(lp, addr_1w[lp]);
    }
    EEPROM.write(0, '#');
  }
}

// Prints the device address to console
#ifdef DEBUG
void dumpAddress(const char *prefix, const OneWireItem *item,
                 const char *postfix) {
  Serial.print(prefix);

  for (int lp = 0; lp < 8; lp++) {
    Serial.print(item->ID[lp] < 16 ? "0" : "");
    Serial.print(item->ID[lp], HEX);
    Serial.print((lp < 7) ? "." : "");
  }

  Serial.println(postfix);
}
#endif