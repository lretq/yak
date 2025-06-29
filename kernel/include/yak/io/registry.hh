#pragma once

#include <yak/io/treenode.hh>
#include <yak/mutex.h>
#include <yak/queue.h>
#include <yak/io/base.hh>
#include <yak/io/device.hh>

struct Personality : public Object {
	IO_OBJ_DECLARE(Personality);

	friend class IoRegistry;

    public:
	virtual const ClassInfo *getDriverClass() const = 0;
	virtual bool isEqual(Object *other) const override = 0;

    private:
	TAILQ_ENTRY(Personality) list_entry;
};

class IoRegistry : public Object {
	IO_OBJ_DECLARE(IoRegistry);

    private:
	void init() override;

    public:
	static IoRegistry &getRegistry();

	Device &rootDevice();

	// match with registered personalities
	Driver *match(Driver *provider, Personality &personality);
	void registerPersonality(Personality &personality);

	void dumpTree();

    private:
	kmutex mutex_;
	TAILQ_HEAD(personality_list, Personality) personalities_;
	Device platform_expert;
};
