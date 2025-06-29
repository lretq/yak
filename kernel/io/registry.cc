#include "yak/log.h"
#include <yak/io/device.hh>
#include <yak/io/guard.hh>
#include <yak/queue.h>
#include <yak/io/registry.hh>

IO_OBJ_DEFINE(IoRegistry, Object);
IO_OBJ_DEFINE_VIRTUAL(Personality, Object);
IO_OBJ_DEFINE(TreeNode, Object);

#define super Object

void IoRegistry::init()
{
	super::init();
	kmutex_init(&mutex_);
	TAILQ_INIT(&personalities_);
	platform_expert.init();
}

static bool registry_init = false;

IoRegistry &IoRegistry::getRegistry()
{
	static IoRegistry singleton;
	if (unlikely(!registry_init)) {
		singleton.init();
		registry_init = true;
	}
	return singleton;
}

Device &IoRegistry::rootDevice()
{
	return platform_expert;
}

Driver *IoRegistry::match(Driver *provider, Personality &personality)
{
	LockGuard lock(mutex_);
	Personality *elm;
	int highest_probe = -10000000;
	Driver *best_driver = nullptr;

	TAILQ_FOREACH(elm, &personalities_, list_entry)
	{
		if (!elm->isEqual(&personality))
			continue;

		auto drv = personality.getDriverClass();
		auto dev = drv->createInstance()->safe_cast<Driver>();
		auto score = dev->probe(provider);
		if (score > highest_probe) {
			highest_probe = score;
			best_driver = dev;
		}
	}

	return best_driver;
}

void IoRegistry::registerPersonality(Personality &personality)
{
	assert(personality.getDriverClass());

	LockGuard lock(mutex_);
	TAILQ_INSERT_TAIL(&this->personalities_, &personality, list_entry);
}

static void dump_level(TreeNode *node, size_t level)
{
	TreeNode *elm;
	TAILQ_FOREACH(elm, &node->children_, list_entry_)
	{
		for (size_t i = 0; i < level; i++)
			printk(0, "\t");
		if (elm->name)
			printk(0, "%s\n", elm->name->getCStr());
		else
			printk(0, "%s\n", "TreeNode");
		dump_level(elm, 1);
	}
}

void IoRegistry::dumpTree()
{
	LockGuard lock(mutex_);
	pr_info("IoRegistry dump:\n");
	dump_level(&platform_expert, 0);
}
