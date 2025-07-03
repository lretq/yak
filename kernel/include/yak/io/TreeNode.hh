#pragma once

#include <cstddef>
#include <yak/io/String.hh>
#include <yak/io/base.hh>
#include <yak/queue.h>
#include <assert.h>

struct TreeNode : public Object {
	IO_OBJ_DECLARE(TreeNode);

    public:
	void init() override
	{
		Object::init();

		parent_ = nullptr;
		TAILQ_INIT(&children_);
	}

	void deinit() override
	{
	}

	void attachChild(TreeNode *child)
	{
		assert(child);
		assert(!child->parent_);
		TAILQ_INSERT_TAIL(&children_, child, list_entry_);
		__atomic_fetch_add(&childcount, 1, __ATOMIC_ACQUIRE);

		child->parent_ = this;

		child->retain();
		this->retain();
	}

	void attachChildAndUnref(TreeNode *child)
	{
		attachChild(child);
		child->release();
	}

	void attachParent(TreeNode *parent)
	{
		parent->attachChild(this);
	}

	String *name = nullptr;

	TreeNode *parent_;

	size_t childcount;
	TAILQ_HEAD(, TreeNode) children_;

	TAILQ_ENTRY(TreeNode) list_entry_;
};
