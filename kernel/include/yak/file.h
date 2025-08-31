#pragma once

#include <yak/refcount.h>
#include <yak/mutex.h>

struct vnode;
struct kprocess;

struct file {
	struct vnode *vnode;
	struct kmutex lock;
	unsigned long refcnt;
	off_t offset;
	unsigned int flags;
};

struct fd {
	struct file *file;
	unsigned int flags;
};

DECLARE_REFMAINT(file);
