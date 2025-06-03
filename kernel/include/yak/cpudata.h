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

	void *kstack_top;

	// initialized via sched_init
	struct spinlock sched_lock;
	struct sched sched;

	kthread_t idle_thread;
	kthread_t *current_thread;
	kthread_t *next_thread;

	unsigned long softint_pending;

	struct spinlock dpc_lock;
	struct list_head dpc_queue;
};

#define curthread() curcpu().current_thread

void cpudata_init(struct cpu *cpu);
