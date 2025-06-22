#pragma once

#include <yak/io/base.hh>
#include <yak/io/device.hh>

class Personality : public Object {
	IO_OBJ_DECLARE(DriverPersonality);

    public:
	virtual const ClassInfo *getDriverClass() const = 0;
	virtual bool isEqual(Object *other) const override = 0;
};

class IoRegistry : public Object {
	IO_OBJ_DECLARE(IoRegistry);

    public:
	static IoRegistry &getRegistry();
};
