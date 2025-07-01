#include <yak/io/pci/Pci.hh>
#include <yak/io/Device.hh>
#include <nanoprintf.h>

IO_OBJ_DEFINE_VIRTUAL(Device, TreeNode);
#define super TreeNode

void Device::init()
{
	super::init();
}

int Device::probe([[maybe_unused]] Device *provider)
{
	return 0;
};

bool Device::start([[maybe_unused]] Device *provider)
{
	return true;
};

void Device::stop([[maybe_unused]] Device *provider) {};

#include <yak/io/IoRegistry.hh>

IO_OBJ_DEFINE(PlatformExpert, Device)
#undef super
#define super Device

bool PlatformExpert::start(Device *provider)
{
	if (!Device::start(provider))
		return false;

	pci_enumerate(this);

	return true;
}
