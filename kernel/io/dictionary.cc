#include <yak/io/dictionary.hh>
#include <yak/macro.h>

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
	String str = String(key);
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

Dictionary::Dictionary(size_t cap)
{
	resize(cap);
}

Dictionary::~Dictionary()
{
	resize(0);
}
