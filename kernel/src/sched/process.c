#include <stdint.h>
#include <yak/process.h>
#include <yak/mutex.h>

struct kprocess kproc0;

static uint64_t next_pid = 0;

void kprocess_init(struct kprocess *process)
{
	process->pid = __atomic_fetch_add(&next_pid, 1, __ATOMIC_SEQ_CST);
	spinlock_init(&process->thread_list_lock);
	LIST_INIT(&process->thread_list);
	process->thread_count = 0;

	if (likely(process != &kproc0)) {
		vm_map_init(&process->map);
	}
}

void uprocess_init(struct kprocess *process)
{
	kprocess_init(process);

	kmutex_init(&process->fd_mutex, "fd");
	process->fd_cap = 0;
	process->fds = NULL;
}
