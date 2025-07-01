#include <stdint.h>
#include <nanoprintf.h>
#include <yak/io/pci.h>
#include <yak/io/pci/Pci.hh>
#include <yak/log.h>
#include <yak/io/pci/PciBus.hh>
#include <yak/io/pci/PciDevice.hh>

#define PCI_UTILS
#include "pci-utils.h"

IO_OBJ_DEFINE(PciDevice, Device);

void PciDevice::init()
{
	Device::init();
}

void PciDevice::initWithPci(uint32_t segment, uint32_t bus, uint32_t slot,
			    uint32_t function)
{
	PciDevice::init();

	personality = PciPersonality(nullptr, vendorId(IPCIARGS),
				     deviceId(IPCIARGS), classCode(IPCIARGS),
				     subClass(IPCIARGS));

	coords.segment = segment;
	coords.bus = bus;
	coords.slot = slot;
	coords.function = function;

	char buf[32];
	npf_snprintf(buf, 32, "PciDev[%d-%d.%02d@%d]", segment, bus, slot,
		     function);
	name = String::fromCStr(buf);
}
