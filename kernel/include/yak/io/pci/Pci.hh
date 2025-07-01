#pragma once

#include <stdint.h>
#include <yak/io/Device.hh>

struct PciCoordinates {
	uint32_t segment, bus, slot, function;
};

void pci_enumerate(Device *provider);
