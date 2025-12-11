#include <yak/heap.h>
#include <yak/init.h>
#include <yak/status.h>
#include <stdint.h>
#include <string.h>
#include <yak/process.h>
#include <yak/mutex.h>
#include <yak/hashtable.h>
#include <yak/cpudata.h>
#include <yak/spinlock.h>

struct kprocess kproc0;

static uint64_t next_pid = 0;

struct spinlock proc_table_lock = SPINLOCK_INITIALIZER();

struct proc_table_entry {
	uint64_t pid;
	struct kprocess *proc;
};

struct proc_table {
	struct proc_table_entry *data;
	size_t len;
	size_t cap;
};

struct proc_table pid_table;

static inline int proc_table_grow(struct proc_table *t)
{
	size_t new_cap = t->cap == 0 ? 16 : t->cap * 2;

	struct proc_table_entry *new_data =
		kmalloc(new_cap * sizeof(struct proc_table_entry));
	if (!new_data)
		return -1;

	kfree(t->data, 0);

	t->data = new_data;
	t->cap = new_cap;
	return 0;
}

void proc_table_init(struct proc_table *t)
{
	t->data = NULL;
	t->len = 0;
	t->cap = 0;
}

status_t proc_table_push(struct proc_table *t, uint64_t pid, struct kprocess *p)
{
	bool state = spinlock_lock_interrupts(&proc_table_lock);

	for (size_t i = 0; i < t->len; i++) {
		if (t->data[i].pid == 0) {
			t->data[i].pid = pid;
			t->data[i].proc = p;
			spinlock_unlock_interrupts(&proc_table_lock, state);
			return YAK_SUCCESS;
		}
	}

	if (t->len == t->cap) {
		if (proc_table_grow(t) != 0) {
			spinlock_unlock_interrupts(&proc_table_lock, state);
			return YAK_OOM;
		}
	}

	t->data[t->len].pid = pid;
	t->data[t->len].proc = p;
	t->len++;

	spinlock_unlock_interrupts(&proc_table_lock, state);
	return YAK_SUCCESS;
}

status_t proc_table_remove(struct proc_table *t, uint64_t pid)
{
	bool state = spinlock_lock_interrupts(&proc_table_lock);

	for (size_t i = 0; i < t->len; i++) {
		if (t->data[i].pid == pid) {
			t->data[i].pid = 0;
			t->data[i].proc = NULL;
			spinlock_unlock_interrupts(&proc_table_lock, state);
			return YAK_SUCCESS;
		}
	}

	spinlock_unlock_interrupts(&proc_table_lock, state);
	return YAK_NOENT;
}

struct kprocess *proc_table_find(struct proc_table *t, uint64_t pid)
{
	bool state = spinlock_lock_interrupts(&proc_table_lock);

	for (size_t i = 0; i < t->len; i++) {
		if (t->data[i].pid == pid) {
			struct kprocess *proc = t->data[i].proc;
			spinlock_unlock_interrupts(&proc_table_lock, state);
			return proc;
		}
	}

	spinlock_unlock_interrupts(&proc_table_lock, state);
	return NULL;
}
struct kprocess *pid_to_proc(uint64_t pid)
{
	return proc_table_find(&pid_table, pid);
}

void kprocess_init(struct kprocess *process)
{
	memset(process, 0, sizeof(struct kprocess));

	process->pid = __atomic_fetch_add(&next_pid, 1, __ATOMIC_SEQ_CST);
	process->parent_process = NULL;

	spinlock_init(&process->thread_list_lock);
	LIST_INIT(&process->thread_list);
	process->thread_count = 0;

	if (likely(process != &kproc0)) {
		process->map = kzalloc(sizeof(struct vm_map));
		vm_map_init(process->map);
	} else {
		process->map = kmap();
	}
}

void uprocess_init(struct kprocess *process, struct kprocess *parent)
{
	kprocess_init(process);

	process->parent_process = parent;

	kmutex_init(&process->fd_mutex, "fd");
	process->fd_cap = 0;
	process->fds = NULL;

	spinlock_init(&process->jobctl_lock);
	process->session.leader = NULL;
	process->session.ctty = NULL;

	process->pgrp_leader = NULL;
	spinlock_init(&process->pgrp_lock);

	proc_table_push(&pid_table, process->pid, process);
}

void proc_init()
{
	proc_table_init(&pid_table);
}

INIT_DEPS(proc);
INIT_ENTAILS(proc);
INIT_NODE(proc, proc_init);
