#ifndef ONEWIRE_COMMON_H
#define ONEWIRE_COMMON_H

//#define DEBUG

#include <OneWireItem.h>

void get_address(uint8_t *addr_1w);
void error_flash(uint16_t ms);
void error_flash(uint16_t flash, uint16_t wait);
void dumpAddress(const char *prefix, const OneWireItem *item, const char *postfix);

#endif