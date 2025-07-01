#include <yak/log.h>
#include <uacpi/utilities.h>
#include <yak/io/Array.hh>
#include <yak/io/base.hh>
#include <yak/io/Device.hh>
#include <yak/io/IoRegistry.hh>
#include <yak/io/acpi/AcpiDevice.hh>
#include <uacpi/resources.h>
#include <nanoprintf.h>

IO_OBJ_DEFINE(AcpiDevice, Device);
#define super Device

void AcpiDevice::initWithArgs(uacpi_namespace_node_info *info)
{
	init();

	if (info->flags & UACPI_NS_NODE_INFO_HAS_HID) {
		auto p = IO_OBJ_CREATE(AcpiPersonality);
		p->initWithArgs(info->hid.value);
		personalities_->push_back(p);
		p->release();

		char buf[32];
		npf_snprintf(buf, sizeof(buf), "AcpiDevice[HID:%s]",
			     info->hid.value);
		name = String::fromCStr(buf);
	}

	if (info->flags & UACPI_NS_NODE_INFO_HAS_CID) {
		for (size_t i = 0; i < info->cid.num_ids; i++) {
			auto p = IO_OBJ_CREATE(AcpiPersonality);
			p->initWithArgs(info->cid.ids[i].value);
			personalities_->push_back(p);
			p->release();
		}
	}

	node_info = info;
}

void AcpiDevice::deinit()
{
	personalities_->release();
	uacpi_free_namespace_node_info(node_info);
}

void AcpiDevice::init()
{
	super::init();
	ALLOC_INIT(personalities_, Array);
}

Array *AcpiDevice::getPersonalities()
{
	return personalities_;
}
