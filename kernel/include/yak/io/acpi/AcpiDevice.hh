#pragma once

#include <yak/io/Device.hh>
#include <yak/io/IoRegistry.hh>
#include <yak/io/acpi/AcpiPersonality.hh>
#include <yak/io/Array.hh>
#include <uacpi/utilities.h>
#include <yak/io/base.hh>

struct AcpiDevice : public Device {
	IO_OBJ_DECLARE(AcpiDevice);

    public:
	Array *getPersonalities() override;

	void init() override;

	void deinit() override;

	void initWithArgs(uacpi_namespace_node *node);

	uacpi_namespace_node *node_ = nullptr;
	uacpi_namespace_node_info *node_info_ = nullptr;

    private:
	Array *personalities_ = nullptr;
};
