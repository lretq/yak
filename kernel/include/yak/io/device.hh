#pragma once

#include <yak/queue.h>
#include <yak/io/base.hh>
#include <yak/io/treenode.hh>
#include <yak/status.h>

class Driver : public TreeNode {
	IO_OBJ_DECLARE(Driver);

    public:
	virtual void init() override;

	virtual int probe(Driver *provider) = 0;
	virtual status_t start(Driver *provider) = 0;
	virtual void stop(Driver *provider) = 0;
};

class Device : public TreeNode {
	IO_OBJ_DECLARE(Device);

    public:
	virtual void init() override;
};
