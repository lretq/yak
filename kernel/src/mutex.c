#include <assert.h>
#include <yak/object.h>
#include <yak/sched.h>
#include <yak/mutex.h>
#include <yak/cpudata.h>
#include <yak/log.h>
#include <yak/kevent.h>

void kmutex_init(struct kmutex *mutex, const char *name)
{
	// initiate as signalled
	kobject_init(&mutex->header, 1);
#ifdef CONFIG_DEBUG
	mutex->name = name;
#endif
	mutex->owner = NULL;
}

static status_t kmutex_acquire_common(struct kmutex *mutex, nstime_t timeout,
				      int waitmode)
{
	assert(mutex);
	assert(mutex->owner != curthread());

	status_t status;

	for (;;) {
		status = sched_wait_single(mutex, waitmode, WAIT_TYPE_ANY,
					   timeout);

		IF_ERR(status)
		{
			return status;
		}

		struct kthread *unlocked = NULL;
		if (likely(__atomic_compare_exchange_n(
			    &mutex->owner, &unlocked, curthread(), 0,
			    __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))) {
			// we now own the mutex
			break;
		}
	}

	return status;
}

status_t kmutex_acquire(struct kmutex *mutex, nstime_t timeout)
{
	return kmutex_acquire_common(mutex, timeout, WAIT_MODE_BLOCK);
}

status_t kmutex_acquire_polling(struct kmutex *mutex, nstime_t timeout)
{
	return kmutex_acquire_common(mutex, timeout, WAIT_MODE_POLL);
}

void kmutex_release(struct kmutex *mutex)
{
	ipl_t ipl = spinlock_lock(&mutex->header.obj_lock);
	assert(mutex->owner == curthread());

	// reset to non-owned
	mutex->header.signalstate = 1;
	mutex->owner = NULL;

	// try to wake one
	kobject_signal_locked(&mutex->header, 0);

	spinlock_unlock(&mutex->header.obj_lock, ipl);
}
