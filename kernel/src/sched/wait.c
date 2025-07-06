#include <assert.h>
#include <yak/log.h>
#include <yak/timer.h>
#include <yak/ipl.h>
#include <yak/spinlock.h>
#include <yak/hint.h>
#include <yak/sched.h>
#include <yak/object.h>
#include <yak/cpudata.h>
#include <yak/queue.h>
#include <yak/types.h>
#include <yak/status.h>

[[gnu::no_instrument_function]]
void sched_wake_thread(struct kthread *thread, status_t status)
{
	assert(spinlock_held(&thread->thread_lock));
	assert(thread->status = THREAD_WAITING);
	thread->wait_status = status;
	sched_resume_locked(thread);
}

[[gnu::no_instrument_function]]
status_t sched_wait_single(void *object, int wait_mode, int wait_type,
			   nstime_t timeout)
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
	} else if (wait_mode == WAIT_MODE_POLL) {
		spinlock_unlock_noipl(&obj->obj_lock);

		status_t ret = YAK_TIMEOUT;
		if (timeout == POLL_ONCE)
			goto poll_exit;

		nstime_t deadline = plat_getnanos() + timeout;

		while (plat_getnanos() < deadline) {
			spinlock_lock_noipl(&obj->obj_lock);

			if (likely(obj->signalstate > 0)) {
				obj->signalstate -= 1;
			}

			spinlock_unlock_noipl(&obj->obj_lock);

			busyloop_hint();
		}

poll_exit:
		spinlock_unlock(&thread->thread_lock, ipl);
		return ret;
	}

	struct wait_block *wb = thread->inline_wait_blocks;
	thread->wait_blocks = wb;

	wb->thread = thread;
	wb->object = object;
	wb->status = YAK_SUCCESS;

	obj->waitcount += 1;
	TAILQ_INSERT_TAIL(&obj->wait_list, wb, entry);

	spinlock_unlock_noipl(&obj->obj_lock);

	thread->wait_type = wait_type;
	thread->status = THREAD_WAITING;

	if (timeout != 0) {
		struct timer *tt = &thread->timeout_timer;
		timer_reset(tt);
		timer_install(tt, timeout);
		tt->hdr.waitcount = 1;
		TAILQ_INSERT_TAIL(&tt->hdr.wait_list,
				  &thread->timeout_wait_block, entry);
	}

	sched_yield(thread, thread->last_cpu);

	if (YAK_TIMEOUT == thread->wait_status) {
		goto exit;
	}

	spinlock_lock_noipl(&obj->obj_lock);
	obj->signalstate -= 1;
	spinlock_unlock_noipl(&obj->obj_lock);

exit:
	spinlock_unlock(&thread->thread_lock, ipl);

	return thread->wait_status;
}
