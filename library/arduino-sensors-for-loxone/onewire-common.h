#ifndef ONEWIRE_COMMON_H
#define ONEWIRE_COMMON_H

#define MAX_ONEWIRE_SENSORS 10

#include <OneWireItem.h>

void get_address(uint8_t *addr_1w);
void dumpAddress(const char *prefix, const OneWireItem *item, const char *postfix);

#endif