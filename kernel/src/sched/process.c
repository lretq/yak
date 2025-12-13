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

struct id_map_entry {
	pid_t id;
	void *ptr;
};

struct id_map {
	struct spinlock table_lock;
	struct id_map_entry *data;
	size_t len;
	size_t cap;
};

struct id_map pid_table;
struct id_map sid_table;
struct id_map pgid_table;

static inline int id_map_grow(struct id_map *t)
{
	size_t new_cap = t->cap == 0 ? 16 : t->cap * 2;

	struct id_map_entry *new_data =
		kzalloc(new_cap * sizeof(struct id_map_entry));
	if (!new_data)
		return -1;

	memcpy(new_data, t->data, t->cap * sizeof(struct id_map_entry));

	kfree(t->data, 0);

	t->data = new_data;
	t->cap = new_cap;
	return 0;
}

static void id_map_init(struct id_map *t)
{
	spinlock_init(&t->table_lock);
	t->data = NULL;
	t->len = 0;
	t->cap = 0;
}

static status_t id_map_push(struct id_map *t, pid_t id, void *p)
{
	bool state = spinlock_lock_interrupts(&t->table_lock);

	// search for free space
	for (size_t i = 0; i < t->len; i++) {
		if (t->data[i].id == 0) {
			t->data[i].id = id;
			t->data[i].ptr = p;
			spinlock_unlock_interrupts(&t->table_lock, state);
			return YAK_SUCCESS;
		}
	}

	if (t->len == t->cap) {
		if (id_map_grow(t) != 0) {
			spinlock_unlock_interrupts(&t->table_lock, state);
			return YAK_OOM;
		}
	}

	t->data[t->len].id = id;
	t->data[t->len].ptr = p;
	t->len++;

	spinlock_unlock_interrupts(&t->table_lock, state);
	return YAK_SUCCESS;
}

static status_t id_map_remove(struct id_map *t, pid_t id)
{
	bool state = spinlock_lock_interrupts(&t->table_lock);

	for (size_t i = 0; i < t->len; i++) {
		if (t->data[i].id == id) {
			t->data[i].id = 0;
			t->data[i].ptr = NULL;
			spinlock_unlock_interrupts(&t->table_lock, state);
			return YAK_SUCCESS;
		}
	}

	spinlock_unlock_interrupts(&t->table_lock, state);
	return YAK_NOENT;
}

static void *id_map_find(struct id_map *t, pid_t pid)
{
	bool state = spinlock_lock_interrupts(&t->table_lock);

	for (size_t i = 0; i < t->len; i++) {
		if (t->data[i].id == pid) {
			void *ptr = t->data[i].ptr;
			spinlock_unlock_interrupts(&t->table_lock, state);
			return ptr;
		}
	}

	spinlock_unlock_interrupts(&t->table_lock, state);
	return NULL;
}

struct kprocess *lookup_pid(pid_t pid)
{
	return id_map_find(&pid_table, pid);
}

struct session *lookup_sid(pid_t sid)
{
	return id_map_find(&sid_table, sid);
}

struct pgrp *lookup_pgid(pid_t pgid)
{
	return id_map_find(&pgid_table, pgid);
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

	process->ppid = parent->pid;
	process->parent_process = parent;

	kmutex_init(&process->fd_mutex, "fd");
	process->fd_cap = 0;
	process->fds = NULL;

	spinlock_init(&process->jobctl_lock);
	process->session = NULL;
	process->pgrp = NULL;

	id_map_push(&pid_table, process->pid, process);
}

void proc_init()
{
	id_map_init(&pid_table);
	id_map_init(&sid_table);
	id_map_init(&pgid_table);
}

INIT_DEPS(proc);
INIT_ENTAILS(proc);
INIT_NODE(proc, proc_init);
