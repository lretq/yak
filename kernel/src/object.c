#include "yak/log.h"
#include <assert.h>
#include <yak/sched.h>
#include <yak/list.h>
#include <yak/object.h>

void kobject_init(struct kobject_header *hdr, int signalstate)
{
	spinlock_init(&hdr->obj_lock);
	hdr->signalstate = signalstate;
	list_init(&hdr->wait_list);
}

void kobject_signal_locked(struct kobject_header *hdr, int unblock_all)
{
	assert(spinlock_held(&hdr->obj_lock));
	struct wait_block *wb;

	while (hdr->waitcount) {
		wb = list_entry(list_pop_front(&hdr->wait_list),
				struct wait_block, wait_list);

		hdr->waitcount -= 1;

		struct kthread *thread = wb->thread;
		spinlock_lock_noipl(&thread->thread_lock);
		sched_wake_thread(thread);
		spinlock_unlock_noipl(&thread->thread_lock);

		if (!unblock_all)
			break;
	}
}
