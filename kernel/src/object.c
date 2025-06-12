#include <assert.h>
#include <yak/sched.h>
#include <yak/queue.h>
#include <yak/object.h>

void kobject_init(struct kobject_header *hdr, int signalstate)
{
	spinlock_init(&hdr->obj_lock);
	hdr->signalstate = signalstate;
	TAILQ_INIT(&hdr->wait_list);
}

void kobject_signal_locked(struct kobject_header *hdr, int unblock_all)
{
	assert(spinlock_held(&hdr->obj_lock));
	struct wait_block *wb;

	while (hdr->waitcount) {
		wb = TAILQ_FIRST(&hdr->wait_list);
		TAILQ_REMOVE(&hdr->wait_list, wb, entry);

		hdr->waitcount -= 1;

		struct kthread *thread = wb->thread;
		spinlock_lock_noipl(&thread->thread_lock);
		sched_wake_thread(thread);
		spinlock_unlock_noipl(&thread->thread_lock);

		if (!unblock_all)
			break;
	}
}
