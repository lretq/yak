#pragma once

#include <yak/queue.h>
#include <yak/io/pci/PciPersonality.hh>
#include <yak/io/pci/Pci.hh>

struct PciDevice : public Device {
	IO_OBJ_DECLARE(PciDevice);

    public:
	Personality &getPersonality() const;

	void init() override;

	void initWithPci(uint32_t segment, uint32_t bus, uint32_t slot,
			 uint32_t function);

    private:
	TAILQ_ENTRY(PciDevice) list_entry;
	PciPersonality personality;
	PciCoordinates coords;
};
