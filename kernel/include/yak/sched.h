#pragma once

#include <stddef.h>
#include <yak/spinlock.h>
#include <yak/arch-sched.h>
#include <yak/list.h>

enum {
	SCHED_PRIO_IDLE = 0,
	SCHED_PRIO_TIME_SHARE = 1, /* 1-16 */
	SCHED_PRIO_TIME_SHARE_END = 16,
	SCHED_PRIO_REAL_TIME = 17, /* 17-32 */
	SCHED_PRIO_REAL_TIME_END = 32,
	SCHED_PRIO_MAX = 32,
};

struct kprocess {
	struct spinlock process_lock;
	size_t thread_count;
	struct list_head thread_list;
};

extern struct kprocess kproc0;

// current/soon-to-be state
enum {
	// enqueued
	THREAD_READY = 1,
	// off-list, active
	THREAD_RUNNING,
	// off-list, set as next
	THREAD_NEXT,
	// off-list, currently switching to thread
	THREAD_SWITCHING,
	// off-list, terminating
	THREAD_TERMINATING,
};

struct kthread {
	struct md_pcb pcb;

	struct spinlock thread_lock;

	unsigned int switching;

	const char *name;

	void *kstack_top;

	unsigned int priority;
	unsigned int status;

	struct cpu *affinity_cpu;
	struct cpu *last_cpu;

	struct kprocess *parent_process;

	struct list_head thread_list, process_list;
};

void kprocess_init(struct kprocess *process);
void kthread_init(struct kthread *thread, const char *name,
		  unsigned int initial_priority, struct kprocess *process);
void kthread_context_init(struct kthread *thread, void *kstack_top,
			  void *entrypoint, void *context1, void *context2);

struct runqueue {
	uint32_t mask;
	struct list_head queue[SCHED_PRIO_MAX];
};

struct sched {
	struct runqueue rqs[2];

	struct runqueue *current_rq;
	struct runqueue *next_rq;

	struct list_head idle_rq;
};

void sched_init();

void sched_insert(struct cpu *cpu, struct kthread *thread, int isOther);

void sched_preempt(struct cpu *cpu);

void sched_resume(struct kthread *thread);
void sched_resume_locked(struct kthread *thread);

void sched_exit_self();
