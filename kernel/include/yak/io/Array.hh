#pragma once

#include <yak/io/base.hh>
#include <yak/macro.h>
#include <assert.h>

class Array : public Object {
	IO_OBJ_DECLARE(Array);

    public:
	class Iterator {
	    public:
		Iterator(const Array *arr, size_t pos)
			: arr(arr)
			, pos(pos)
		{
		}

		Object *operator*() const
		{
			return arr->entries[pos];
		}

		Iterator &operator++()
		{
			++pos;
			return *this;
		}

		bool operator!=(const Iterator &other) const
		{
			return pos != other.pos;
		}

	    private:
		const Array *arr;
		size_t pos;
	};

	void init() override
	{
		Object::init();
		entries = nullptr;
		count = capacity = 0;
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
		if (new_size == capacity)
			return;

		if (new_size < count) {
			for (size_t i = new_size; i < count; i++)
				entries[i]->release();
			count = new_size;
		}

		Object **new_entries = nullptr;
		if (new_size > 0) {
			new_entries = new Object *[new_size];
			size_t copy_count = MIN(count, new_size);
			if (entries)
				memcpy(new_entries, entries,
				       sizeof(Object *) * copy_count);
			for (size_t i = copy_count; i < new_size; i++)
				new_entries[i] = nullptr;
		}

		delete[] entries;
		entries = new_entries;
		capacity = new_size;
	}

	size_t length() const
	{
		return count;
	}

	void push_back(Object *obj)
	{
		if (count >= capacity)
			resize(capacity == 0 ? 4 : capacity * 2);
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
