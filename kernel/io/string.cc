#include <yak/io/string.hh>

IO_OBJ_DEFINE(String, Object);

String::String(const char *str, size_t length)
{
	data_ = new char[length + 1];
	memcpy(data_, str, length);
	data_[length] = '\0';
	length_ = length;
}

String::String(const char *str)
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
	return new String(c_str);
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
