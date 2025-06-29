#include <stdint.h>
#include <nanoprintf.h>
#include <yak/io/pci.h>
#include <yak/io/pci.hh>
#include <yak/log.h>

IO_OBJ_DEFINE(PciPersonality, Personality);
IO_OBJ_DEFINE(PciDevice, Device);

#define PCIARGS uint32_t segment, uint32_t bus, uint32_t slot, uint32_t function
#define IPCIARGS segment, bus, slot, function

uint16_t vendorId(PCIARGS)
{
	auto reg0 = pci_read16(IPCIARGS, 0);
	return (uint16_t)reg0;
}

uint64_t deviceId(PCIARGS)
{
	auto reg0 = pci_read32(IPCIARGS, 0);
	return (uint16_t)(reg0 >> 16);
}

uint8_t classCode(PCIARGS)
{
	auto reg3 = pci_read32(IPCIARGS, 0x8);
	return (uint8_t)(reg3 >> 24);
}

uint8_t subClass(PCIARGS)
{
	auto reg3 = pci_read32(IPCIARGS, 0x8);
	return (uint8_t)(reg3 >> 16);
}

uint8_t revisionId(PCIARGS)
{
	auto reg3 = pci_read8(IPCIARGS, 0x8);
	return reg3;
}

uint8_t headerType(PCIARGS)
{
	auto reg = pci_read32(IPCIARGS, 0xc);
	return (uint8_t)(reg >> 16);
}

struct PciBus : public Device {
	IO_OBJ_DECLARE(PciBus);

    public:
	virtual void init() override;

	uint32_t segId, busId;

	uint32_t seg, bus, slot, function;
};
IO_OBJ_DEFINE(PciBus, Device);

void PciBus::init()
{
	Device::init();
	name = String::fromCStr("PciBus");
}

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

void bus_enum(PciBus *busObj, uint32_t seg, uint32_t bus);

void fn_enum(PciBus *busObj, PCIARGS)
{
	auto cc = classCode(IPCIARGS);
	auto sc = subClass(IPCIARGS);

	if ((cc == 0x6) && (sc == 0x0))
		return;

	if ((cc == 0x6) && (sc == 0x4)) {
		// == bus
		return;
	}

	assert((headerType(IPCIARGS) & ~0x80) == 0x0);

	PciDevice *dev = IO_OBJ_CREATE(PciDevice);
	dev->initWithPci(IPCIARGS);

	pr_info("enum pci function at %d-%d.%02d@%d\n", segment, bus, slot,
		function);
	busObj->attachChild(dev);
}

void dev_enum(PciBus *busObj, uint32_t segment, uint32_t bus, uint32_t slot)
{
	uint16_t vid;
	uint8_t function = 0;

	vid = vendorId(IPCIARGS);
	if (vid == 0xFFFF) {
		return;
	}

	fn_enum(busObj, IPCIARGS);
	if ((headerType(IPCIARGS) & 0x80) != 0) {
		for (function = 1; function < 8; function++) {
			if (vendorId(IPCIARGS) == 0xFFFF)
				break;
			fn_enum(busObj, IPCIARGS);
		}
	}
}

void bus_enum(PciBus *busObj, uint32_t seg, uint32_t bus)
{
	pr_info("enumerate bus %d-%d\n", seg, bus);
	for (uint8_t device = 0; device < 32; device++) {
		dev_enum(busObj, seg, bus, device);
	}
}

void pci_enumerate()
{
	auto header = headerType(0, 0, 0, 0);
	if ((header & 0x80) == 0) {
		PciBus *dev;
		ALLOC_INIT(dev, PciBus);

		dev->segId = dev->busId = 0;
		dev->seg = 0;
		dev->bus = 0;
		dev->slot = 0;
		dev->function = 0;

		IoRegistry::getRegistry().rootDevice().attachChild(dev);
		bus_enum(dev, dev->segId, dev->busId);
	} else {
		for (int function = 0; function < 8; function++) {
			if (vendorId(0, 0, 0, function) == 0xFFFF)
				break;
			PciBus *dev;
			ALLOC_INIT(dev, PciBus);

			dev->segId = 0;
			dev->busId = function;

			dev->seg = 0;
			dev->bus = 0;
			dev->slot = 0;
			dev->function = function;

			bus_enum(dev, dev->segId, dev->busId);
		}
	}
}
