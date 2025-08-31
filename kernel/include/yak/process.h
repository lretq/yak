#pragma once

#include <stdint.h>
#include <stddef.h>
#include <yak/queue.h>
#include <yak/spinlock.h>
#include <yak/file.h>
#include <yak/vm/map.h>

struct kprocess {
	uint64_t pid;

	struct spinlock thread_list_lock;
	size_t thread_count;
	LIST_HEAD(, kthread) thread_list;

	struct kmutex fd_mutex;
	int fd_cap;
	struct fd **fds;

	struct vm_map map;
};

// it's me, hi, im the problem its me
extern struct kprocess kproc0;

// initialize the kernel part of a process
void kprocess_init(struct kprocess *process);

// initialize the kernel+user parts of a process
void uprocess_init(struct kprocess *process);
