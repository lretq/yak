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

static void print_node(TreeNode *elm)
{
	if (elm->name)
		printk(0, "%s\n", elm->name->getCStr());
	else
		printk(0, "%s\n", elm->getClassInfo()->className);
}

static void dump_level(TreeNode *node, size_t level, bool is_last = false)
{

	for (size_t i = 0; i < level; i++)
		printk(0, "\t");

	if (level != 0)
		printk(0, "%s", is_last ? "└── " : "├── ");

	print_node(node);

	TreeNode *elm;
	size_t j = 0;
	TAILQ_FOREACH(elm, &node->children_, list_entry_)
	{
		j++;
		dump_level(elm, level + 1, j == node->childcount);
	}
}

void IoRegistry::dumpTree()
{
	LockGuard lock(mutex_);
	printk(0, "=== IoRegistry Dump ===\n");
	dump_level(&platform_expert, 0);
	printk(0, "=== IoRegistry Dump end ===\n");
}
