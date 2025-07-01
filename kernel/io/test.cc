#include <yak/io/pci/Pci.hh>
#include <yak/io/base.hh>
#include <yak/io/Device.hh>
#include <yak/io/Dictionary.hh>
#include <yak/io/String.hh>
#include <yak/io/pci/PciPersonality.hh>
#include <yak/log.h>

class TestDevice final : public Device {
	IO_OBJ_DECLARE(TestDevice);

    public:
	int probe(Device *provider) override
	{
		(void)provider;
		return 1000;
	}

	bool start(Device *provider) override
	{
		(void)provider;
		return true;
	}

	void stop(Device *provider) override
	{
		(void)provider;
	};
};

IO_OBJ_DEFINE(TestDevice, Device);

PciPersonality testDevPers =
	PciPersonality(&TestDevice::classInfo, PciPersonality::MATCH_ANY,
		       PciPersonality::MATCH_ANY, PciPersonality::MATCH_ANY,
		       PciPersonality::MATCH_ANY);

extern "C" void iotest_fn()
{
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

	IoRegistry::getRegistry().getExpert().start(nullptr);

	IoRegistry::getRegistry().dumpTree();
}
