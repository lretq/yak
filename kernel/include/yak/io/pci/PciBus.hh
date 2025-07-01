#pragma once

#include <yak/io/base.hh>
#include <yak/io/Device.hh>
#include <stdint.h>

struct PciBus : public Device {
	IO_OBJ_DECLARE(PciBus);

    public:
	void init() override;

	void initWithArgs(uint32_t segId, uint32_t busId, uint32_t seg,
			  uint32_t bus, uint32_t slot, uint32_t function,
			  PciBus *parentBus = nullptr);

	bool start(Device *provider) override;

	PciBus *parentBus;

	uint32_t segId, busId;

	uint32_t seg, bus, slot, function;
};
