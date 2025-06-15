#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <yak/status.h>
#include <yak/spinlock.h>
#include <yak/arch-sched.h>
#include <yak/queue.h>
#include <yak/timer.h>
#include <yak/types.h>

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
	LIST_HEAD(, kthread) thread_list;
};

extern struct kprocess kproc0;

struct wait_block {
	// thread waiting
	struct kthread *thread;
	// object being waited on
	void *object;
	// status to set in the thread for WAIT_TYPE_ANY
	status_t status;
	// for inserting into object wait list
	TAILQ_ENTRY(wait_block) entry;
};

enum {
	/* unblock when any object is ready */
	WAIT_TYPE_ANY = 1,
	/* unblock when all objects are ready */
	WAIT_TYPE_ALL = 2,
};

enum {
	WAIT_MODE_BLOCK = 1,
	WAIT_MODE_POLL,
};

#define KTHREAD_INLINE_WAIT_BLOCKS 4

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
	// off-list, blocked
	THREAD_WAITING,
	// off-list, freshly created
	THREAD_UNDEFINED,
};

#define KTHREAD_MAX_NAME_LEN 32

struct kthread {
	struct md_pcb pcb;

	struct spinlock thread_lock;

	unsigned int switching;

	char name[KTHREAD_MAX_NAME_LEN];

	void *kstack_top;

	struct wait_block inline_wait_blocks[KTHREAD_INLINE_WAIT_BLOCKS];
	struct wait_block *wait_blocks;
	unsigned int wait_type;
	status_t wait_status;

	struct wait_block timeout_wait_block;
	struct timer timeout_timer;

	unsigned int priority;
	unsigned int status;

	struct cpu *affinity_cpu;
	struct cpu *last_cpu;

	struct kprocess *parent_process;

	LIST_ENTRY(kthread) process_entry;
	TAILQ_ENTRY(kthread) thread_entry;
};

void kprocess_init(struct kprocess *process);
void kthread_init(struct kthread *thread, const char *name,
		  unsigned int initial_priority, struct kprocess *process);

status_t kernel_thread_create(const char *name, unsigned int priority,
			      void *entry, void *context, int instant_launch,
			      struct kthread **out);

void kthread_context_init(struct kthread *thread, void *kstack_top,
			  void *entrypoint, void *context1, void *context2);

TAILQ_HEAD(thread_list, kthread);

struct runqueue {
	uint32_t mask;
	struct thread_list queue[SCHED_PRIO_MAX];
};

struct sched {
	struct runqueue rqs[2];

	struct runqueue *current_rq;
	struct runqueue *next_rq;

	struct thread_list idle_rq;
};

void sched_init();

void sched_insert(struct cpu *cpu, struct kthread *thread, int isOther);

void sched_preempt(struct cpu *cpu);

void sched_resume(struct kthread *thread);
void sched_resume_locked(struct kthread *thread);

[[gnu::noreturn]]
void sched_exit_self();

void sched_yield(struct kthread *current, struct cpu *cpu);

void sched_wake_thread(struct kthread *thread, status_t status);

#define TIMEOUT_INFINITE 0
#define POLL_ONCE 0

status_t sched_wait_single(void *object, int wait_mode, int wait_type,
			   nstime_t timeout);

#ifdef __cplusplus
}
#endif
