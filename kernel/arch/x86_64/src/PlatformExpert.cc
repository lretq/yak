#include <uacpi/uacpi.h>
#include <uacpi/resources.h>
#include <uacpi/utilities.h>
#include <yak/log.h>
#include <yak/io/base.hh>
#include <yak/io/pci/Pci.hh>
#include <yak/io/acpi/AcpiDevice.hh>
#include <yak/io/IoRegistry.hh>

IO_OBJ_DEFINE(PlatformExpert, Device)
#define super Device

static uacpi_iteration_decision
acpi_ns_enum(void *provider, uacpi_namespace_node *node,
	     [[maybe_unused]] uacpi_u32 node_depth)
{
	uacpi_namespace_node_info *node_info;

	uacpi_status ret = uacpi_get_namespace_node_info(node, &node_info);
	if (uacpi_unlikely_error(ret)) {
		const char *path =
			uacpi_namespace_node_generate_absolute_path(node);
		pr_error("unable to retrieve node %s information: %s", path,
			 uacpi_status_to_string(ret));
		uacpi_free_absolute_path(path);
		return UACPI_ITERATION_DECISION_CONTINUE;
	}

	auto device = IO_OBJ_CREATE(AcpiDevice);
	device->initWithArgs(node_info);
	((Device *)provider)->attachChild(device);
	device->release();

	return UACPI_ITERATION_DECISION_CONTINUE;
}

struct AcpiNamespace : public Device {
	IO_OBJ_DECLARE(AcpiNamespace);

    public:
	bool start(Device *provider) override
	{
		if (!Device::start(provider))
			return false;

		uacpi_namespace_for_each_child(uacpi_namespace_root(),
					       acpi_ns_enum, UACPI_NULL,
					       UACPI_OBJECT_DEVICE_BIT,
					       UACPI_MAX_DEPTH_ANY, this);

		return true;
	}
};

IO_OBJ_DEFINE(AcpiNamespace, Device);

bool PlatformExpert::start(Device *provider)
{
	if (!Device::start(provider))
		return false;

	AcpiNamespace *acpi_root_dev;
	ALLOC_INIT(acpi_root_dev, AcpiNamespace);
	this->attachChild(acpi_root_dev);
	acpi_root_dev->start(this);
	acpi_root_dev->release();

	pci_enumerate(this);

	return true;
}
