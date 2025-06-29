#include <yak/io/dictionary.hh>
#include <yak/macro.h>
#include <assert.h>
#include <yak/log.h>

IO_OBJ_DEFINE(Dictionary, Object);

void Dictionary::resize(size_t new_size)
{
	if (new_size == 0) {
		if (entries)
			delete[] entries;
		entries = nullptr;
	} else {
		auto new_entries = new Entry[new_size];
		pr_info("%p\n", new_entries);
		assert(new_entries);
		if (entries) {
			memcpy(new_entries, entries,
			       sizeof(Entry) * MIN(new_size, this->count));
			delete[] entries;
		}
		entries = new_entries;
	}
	count = MIN(count, new_size);
	capacity = new_size;
}

bool Dictionary::insert(const char *key, Object *value)
{
	return insert(String::fromCStr(key), value);
}

bool Dictionary::insert(String *key, Object *value)
{
	if (!entries || count >= capacity) {
		if (capacity == 0)
			resize(4);
		else
			resize(capacity * 2);
	}
	for (size_t i = 0; i < count; i++) {
		if (key->isEqual(entries[i].key))
			return false;
	}
	entries[count++] = { key, value };
	return true;
}

bool Dictionary::isEqual(Object *object) const
{
	if (auto dict = object->safe_cast<Dictionary>()) {
		if (this->count != dict->count)
			return false;

		for (auto v : *dict) {
			auto el = lookup(v.key);
			if (!el)
				return false;

			if (!el->isEqual(v.value))
				return false;
		}
	}

	return true;
}

Object *Dictionary::lookup(const char *key) const
{
	String str = String();
	str.init(key);
	return lookup(&str);
}

Object *Dictionary::lookup(String *key) const
{
	for (size_t i = 0; i < count; i++) {
		if (key->isEqual(entries[i].key))
			return entries[i].value;
	}
	return nullptr;
}

void Dictionary::init()
{
	Object::init();
}

void Dictionary::initWithSize(size_t cap)
{
	resize(cap);
}

void Dictionary::deinit()
{
	resize(0);
}
