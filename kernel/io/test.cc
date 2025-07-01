#include "yak/io/acpi/AcpiPersonality.hh"
#include <yak/io/pci/Pci.hh>
#include <yak/io/base.hh>
#include <yak/io/Device.hh>
#include <yak/io/Dictionary.hh>
#include <yak/io/String.hh>
#include <yak/io/pci/PciPersonality.hh>
#include <yak/log.h>

class Ps2Kbd final : public Device {
	IO_OBJ_DECLARE(Ps2Kbd);

    public:
	int probe(Device *provider) override
	{
		(void)provider;
		return 1000;
	}

	bool start(Device *provider) override
	{
		(void)provider;
		pr_info("start ps2 keyboard ...\n");
		return true;
	}

	void stop(Device *provider) override
	{
		(void)provider;
	};
};

IO_OBJ_DEFINE(Ps2Kbd, Device);

AcpiPersonality ps2kbdPers = AcpiPersonality(&Ps2Kbd::classInfo, "PNP0303");

typedef void (*func_ptr)(void);
extern func_ptr __init_array[];
extern func_ptr __init_array_end[];

void run_init_array()
{
	for (func_ptr *func = __init_array; func < __init_array_end; ++func) {
		(*func)(); // Call constructor
	}
}

extern "C" void iotest_fn()
{
	run_init_array();

	auto dict = new Dictionary();
	dict->initWithSize(4);
	dict->insert("testKey1", String::fromCStr("Hello"));
	dict->insert("testKey2", String::fromCStr("World"));
	dict->insert("testKey3", String::fromCStr("From"));
	dict->insert("testKey4", String::fromCStr("Dict"));
	for (auto v : *dict) {
		auto str = v.value->safe_cast<String>();
		if (str) {
			pr_info("%s -> %s\n", v.key->getCStr(), str->getCStr());
		}
	}

	pr_info("lookup %s: %s\n", "testKey1",
		dict->lookup("testKey1")->safe_cast<String>()->getCStr());

	auto &reg = IoRegistry::getRegistry();
	reg.getExpert().start(nullptr);
	//reg.dumpTree();

	reg.registerPersonality(&ps2kbdPers);

	reg.matchAll();
}
