#pragma once

#include <yak/io/base.hh>
#include <yak/io/device.hh>

struct DriverPersonality {
	const char *key;
	const void *value;
	const ClassInfo *driverClass;
};

class IoRegistry : public Object {
	IO_OBJ_DECLARE(IoRegistry);

    public:
	static IoRegistry &getRegistry();
};
