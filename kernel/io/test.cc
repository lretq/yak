#include <cstddef>
#include <yak/status.h>
#include <yak/io/base.hh>
#include <yak/io/device.hh>
#include <yak/macro.h>
#include <yak/log.h>

// superclass of all classes
IO_OBJ_DEFINE_ROOT(Object);

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

class String : public Object {
	IO_OBJ_DECLARE(String);

    public:
	String()
	{
	}

	String(const char *str)
	{
		buf = str;
		len = strlen(buf);
		allocated = false;
	}

	String(const char *str, size_t len)
	{
		auto buf = new char[len];
		strncpy(buf, str, len);
		this->len = len;
		this->buf = buf;
		allocated = true;
	}

	virtual ~String()
	{
		if (allocated)
			delete[] buf;
	}

	bool isEqual(String *str) const
	{
		return strcmp(this->getCStr(), str->getCStr()) == 0;
	}

	static String *fromCStr(const char *c_str);

	const char *getCStr() const;

    private:
	bool allocated = false;
	const char *buf = nullptr;
	size_t len = 0;
};

IO_OBJ_DEFINE(String, Object);

String *String::fromCStr(const char *c_str)
{
	return new String(c_str, strlen(c_str) + 1);
}

const char *String::getCStr() const
{
	return buf;
}

class Dictionary : public Object {
	IO_OBJ_DECLARE(Dictionary);

    private:
	struct Entry {
		String *key;
		Object *value;
	};

	class Iterator {
	    public:
		Iterator(const Dictionary *dict, size_t pos)
			: dict(dict)
			, current(pos)
		{
		}

		Entry operator*() const
		{
			return dict->entries[current];
		}

		Iterator &operator++()
		{
			++current;
			return *this;
		}

		bool operator!=(const Iterator &other) const
		{
			return current != other.current;
		}

	    private:
		const Dictionary *dict;
		size_t current;
	};

    public:
	Dictionary(size_t cap = 4);
	virtual ~Dictionary();
	Dictionary(Dictionary &&) = delete;
	Dictionary(const Dictionary &) = delete;
	Dictionary &operator=(Dictionary &&) = delete;
	Dictionary &operator=(const Dictionary &) = delete;

	void resize(size_t new_size);
	// returns true if inserted
	// returns false if key exists already
	bool insert(const char *key, Object *value);
	bool insert(String *key, Object *value);

	Object *lookup(const char *key);

	Iterator begin() const
	{
		return Iterator(this, 0);
	}

	Iterator end() const
	{
		return Iterator(this, count);
	}

    private:
	Entry *entries = nullptr;
	size_t count = 0;
	size_t capacity = 0;
};

IO_OBJ_DEFINE(Dictionary, Object);

void Dictionary::resize(size_t new_size)
{
	auto new_entries = new Entry[new_size];
	memcpy(new_entries, entries,
	       sizeof(Entry) * MIN(new_size, this->count));
	delete[] entries;
	entries = new_entries;
	capacity = new_size;
}

bool Dictionary::insert(const char *key, Object *value)
{
	return insert(String::fromCStr(key), value);
}

bool Dictionary::insert(String *key, Object *value)
{
	for (size_t i = 0; i < count; i++) {
		if (key->isEqual(entries[i].key))
			return false;
	}
	entries[count++] = { key, value };
	return true;
}

Object *Dictionary::lookup(const char *key)
{
	String str = String(key);
	for (size_t i = 0; i < count; i++) {
		if (str.isEqual(entries[i].key))
			return entries[i].value;
	}
	return NULL;
}

Dictionary::Dictionary(size_t cap)
{
	resize(cap);
}

Dictionary::~Dictionary()
{
	resize(0);
}

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
