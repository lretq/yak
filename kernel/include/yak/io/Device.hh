#pragma once

#include <yak/queue.h>
#include <yak/io/base.hh>
#include <yak/io/TreeNode.hh>
#include <yak/io/Array.hh>

struct Personality;

class Device : public TreeNode {
	IO_OBJ_DECLARE(Device);

    public:
	virtual void init() override;

	virtual Array *getPersonalities()
	{
		return nullptr;
	}

	virtual Personality *getPersonality()
	{
		return nullptr;
	}

	virtual int probe(Device *provider);
	virtual bool start(Device *provider);
	virtual void stop(Device *provider);
};

class PlatformExpert : public Device {
	IO_OBJ_DECLARE(PlatformExpert);

    public:
	bool start(Device *provider) override;
};
