#include <yak/io/base.hh>
#include <yak/io/device.hh>
#include <yak/io/dictionary.hh>
#include <yak/io/string.hh>
#include <yak/log.h>

class TestDevice final : public Device {
	IO_OBJ_DECLARE(TestObj);

    public:
	int probe(Device *provider) override
	{
		(void)provider;
		return 1000;
	}

	status_t start(Device *provider) override
	{
		(void)provider;
		return YAK_SUCCESS;
	}

	void stop(Device *provider) override
	{
		(void)provider;
	};
};

IO_OBJ_DEFINE(TestDevice, Device);

extern "C" void iotest_fn()
{
	auto dict = new Dictionary();
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
}
