#pragma once

#include <stdint.h>
#include <stddef.h>
#include <yak/queue.h>
#include <yak/spinlock.h>
#include <yak/vm/map.h>

struct kprocess {
	uint64_t pid;

	struct spinlock process_lock;

	size_t thread_count;
	LIST_HEAD(, kthread) thread_list;

	struct vm_map map;
};

// it's me, hi, im the problem its me
extern struct kprocess kproc0;
