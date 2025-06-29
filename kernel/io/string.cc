#include <yak/io/string.hh>

IO_OBJ_DEFINE(String, Object);
#define super Object

void String::init()
{
	super::init();
}

void String::init(const char *str, size_t length)
{
	data_ = new char[length + 1];
	memcpy(data_, str, length);
	data_[length] = '\0';
	length_ = length;
}

void String::init(const char *str)
{
	if (!str) {
		data_ = nullptr;
		length_ = 0;
		return;
	}
	length_ = strlen(str);
	data_ = new char[length_ + 1];
	memcpy(data_, str, length_ + 1);
}

String *String::fromCStr(const char *c_str)
{
	auto obj = new String();
	obj->init(c_str);
	return obj;
}

const char *String::getCStr() const
{
	return data_;
}

bool String::isEqual(Object *other) const
{
	if (auto str = other->safe_cast<String>()) {
		return strcmp(getCStr(), str->getCStr()) == 0;
	}
	return false;
}

size_t String::length() const
{
	return length_;
}
