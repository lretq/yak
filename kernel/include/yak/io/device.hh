#pragma once

#include <yak/io/base.hh>
#include <yak/status.h>

class Device : public Object {
	IO_OBJ_DECLARE(Device);

    public:
	virtual int probe(Device *provider) = 0;
	virtual status_t start(Device *provider) = 0;
	virtual void stop(Device *provider) = 0;
};
