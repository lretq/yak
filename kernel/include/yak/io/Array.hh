#pragma once

#include <yak/io/base.hh>
#include <yak/macro.h>
#include <assert.h>

class Array : public Object {
	IO_OBJ_DECLARE(Array);

    public:
	class Iterator {
	    public:
		Iterator(const Array *dict, size_t pos)
			: dict(dict)
			, current(pos)
		{
		}

		Object *operator*() const
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
		const Array *dict;
		size_t current;
	};

	void init() override
	{
		Object::init();
	}

	void deinit() override
	{
		resize(0);
		Object::deinit();
	}

	void initWithCapacity(size_t cap = 4)
	{
		init();
		resize(cap);
	}

	Array(Array &&) = delete;
	Array(const Array &) = delete;
	Array &operator=(Array &&) = delete;
	Array &operator=(const Array &) = delete;

	void resize(size_t new_size)
	{
		if (new_size < count) {
			for (size_t i = new_size; i < count; i++) {
				entries[i]->release();
			}
		}

		if (new_size == 0) {
			if (entries)
				delete[] entries;
			entries = nullptr;
		} else {
			auto new_entries = new Object *[new_size];
			assert(new_entries);
			if (entries) {
				memcpy(new_entries, entries,
				       sizeof(Object *) *
					       MIN(new_size, this->count));
				delete[] entries;
			}
			entries = new_entries;
		}
	}

	size_t length() const
	{
		return count;
	}

	void push_back(Object *obj)
	{
		if (count + 1 >= capacity)
			resize(capacity + 1);
		obj->retain();
		entries[count++] = obj;
	}

	Object **getCArray() const
	{
		return entries;
	}

	Iterator begin() const
	{
		return Iterator(this, 0);
	}

	Iterator end() const
	{
		return Iterator(this, count);
	}

    private:
	Object **entries = nullptr;
	size_t count = 0;
	size_t capacity = 0;
};
