#pragma once

#include <stdint.h>

void lapic_eoi();
void lapic_defer_interrupt(uint8_t number);
