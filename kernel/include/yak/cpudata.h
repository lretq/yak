#pragma once

#include <stddef.h>
#include <yak/ipl.h>
#include <yak/list.h>
#include <yak/percpu.h>
#include <yak/arch-cpudata.h>
#include <yak/sched.h>
#include <yak/spinlock.h>

struct cpu {
	struct cpu_md md;
	struct cpu *self;

	size_t cpu_id;

	// initialized via sched_init
	struct sched sched;

	kthread_t *current_thread;
	kthread_t *next_thread;

	unsigned long softint_pending;

	struct spinlock dpc_lock;
	struct list_head dpc_queue;
};

void cpudata_init(struct cpu *cpu);
