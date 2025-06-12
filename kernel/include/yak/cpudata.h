#pragma once

#include <stddef.h>
#include <yak/ipl.h>
#include <yak/queue.h>
#include <yak/percpu.h>
#include <yak/arch-cpudata.h>
#include <yak/sched.h>
#include <yak/spinlock.h>

struct cpu {
	struct cpu_md md;
	struct cpu *self;

	size_t cpu_id;

	void *kstack_top;

	// initialized via sched_init
	struct spinlock sched_lock;
	struct sched sched;

	struct kthread idle_thread;
	struct kthread *current_thread;
	struct kthread *next_thread;

	unsigned long softint_pending;

	struct spinlock dpc_lock;
	LIST_HEAD(, dpc) dpc_queue;
};

#define curthread() curcpu().current_thread

void cpudata_init(struct cpu *cpu);
