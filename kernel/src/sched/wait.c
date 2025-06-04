#include <assert.h>
#include <yak/log.h>
#include <yak/ipl.h>
#include <yak/spinlock.h>
#include <yak/hint.h>
#include <yak/sched.h>
#include <yak/object.h>
#include <yak/cpudata.h>

void sched_wake_thread(struct kthread *thread)
{
	assert(spinlock_held(&thread->thread_lock));
	assert(thread->status = THREAD_WAITING);
	sched_resume_locked(thread);
}

status_t sched_wait_single(void *object)
{
	struct kthread *thread = curthread();
	struct kobject_header *obj = object;

	ipl_t ipl = spinlock_lock(&thread->thread_lock);

	spinlock_lock_noipl(&obj->obj_lock);

	// fast path
	if (likely(obj->signalstate > 0)) {
		obj->signalstate -= 1;
		spinlock_unlock_noipl(&obj->obj_lock);
		spinlock_unlock(&thread->thread_lock, ipl);
		return YAK_SUCCESS;
	}

	struct wait_block *wb = thread->inline_wait_blocks;
	thread->wait_blocks = wb;

	wb->thread = thread;
	wb->object = object;
	list_init(&wb->wait_list);

	obj->waitcount += 1;
	list_add_tail(&obj->wait_list, &wb->wait_list);

	spinlock_unlock_noipl(&obj->obj_lock);

	thread->wait_type = THREAD_WAITING;

	sched_yield(thread, thread->last_cpu);

	spinlock_lock_noipl(&obj->obj_lock);
	obj->signalstate -= 1;
	spinlock_unlock_noipl(&obj->obj_lock);

	spinlock_unlock(&thread->thread_lock, ipl);

	return YAK_SUCCESS;
}
