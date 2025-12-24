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

static void waitlist_insert(struct wait_block *wb);

void sched_wake_thread(struct kthread *thread, status_t status)
{
	assert(spinlock_held(&thread->thread_lock));
	assert(thread->status = THREAD_WAITING);
	thread->wait_status = status;
	sched_resume_locked(thread);
}

status_t sched_wait_single(void *object, int wait_mode, int wait_type,
			   nstime_t timeout)
{
	assert(object);
	struct kthread *thread = curthread();
	struct kobject_header *obj = object;

	ipl_t ipl = spinlock_lock(&obj->obj_lock);

	// fast path
	if (likely(obj->signalstate > 0)) {
		obj->signalstate -= 1;
		spinlock_unlock(&obj->obj_lock, ipl);
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
				ret = YAK_SUCCESS;
				goto poll_exit;
			}

			spinlock_unlock_noipl(&obj->obj_lock);

			busyloop_hint();
		}

poll_exit:
		xipl(ipl);
		return ret;
	} else {
		assert(curthread() != &curcpu_ptr()->idle_thread);
	}

	spinlock_lock_noipl(&thread->thread_lock);

	struct wait_block *wb = &thread->inline_wait_blocks[0];
	thread->wait_blocks = wb;

	wb->thread = thread;
	wb->object = object;
	wb->status = YAK_SUCCESS;

	assert(obj->wait_list.tqh_last);
	TAILQ_INSERT_TAIL(&obj->wait_list, wb, entry);
	obj->waitcount += 1;

	spinlock_unlock_noipl(&obj->obj_lock);

	thread->wait_type = wait_type;
	thread->status = THREAD_WAITING;

	if (timeout != 0) {
		struct timer *tt = &thread->timeout_timer;
		timer_reset(tt);
		timer_install(tt, timeout);

		// We cannot miss the timeout, we are at ipl = DPC
		tt->hdr.waitcount = 1;
		TAILQ_INSERT_TAIL(&tt->hdr.wait_list,
				  &thread->timeout_wait_block, entry);
	}

	sched_yield(thread, thread->last_cpu);
	assert(!spinlock_held(&thread->thread_lock));

	// TODO: uninstall timer
	// Problem: what if timeout is over, but the thread already woke up?
	assert(timeout == 0);

	if (YAK_TIMEOUT == thread->wait_status) {
		goto exit;
	}

	spinlock_lock_noipl(&obj->obj_lock);
	obj->signalstate -= 1;
	spinlock_unlock_noipl(&obj->obj_lock);

exit:
	xipl(ipl);

	return thread->wait_status;
}
