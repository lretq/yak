#pragma once

#include <stddef.h>
#include <yak/spinlock.h>
#include <yak/queue.h>

struct kobject_header {
	struct spinlock obj_lock;
	// <=0 unsignaled, >0 signaled
	long signalstate;
	// number of threads waiting on object
	size_t waitcount;
	// list of wait_blocks
	TAILQ_HEAD(, wait_block) wait_list;
};

void kobject_init(struct kobject_header *hdr, int signalstate);

void kobject_signal_locked(struct kobject_header *hdr, int unblock_all);
