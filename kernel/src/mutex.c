#include <assert.h>
#include <yak/object.h>
#include <yak/sched.h>
#include <yak/mutex.h>
#include <yak/cpudata.h>
#include <yak/log.h>
#include <yak/kevent.h>

void kmutex_init(struct kmutex *mutex, const char *name)
{
	event_init(&mutex->event, 0);
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

	do {
		struct kthread *unlocked = NULL;
		if (likely(__atomic_compare_exchange_n(
			    &mutex->owner, &unlocked, curthread(), 0,
			    __ATOMIC_ACQ_REL, __ATOMIC_RELAXED))) {
			// we now own the mutex
			return YAK_SUCCESS;
		}

		status = sched_wait_single(mutex, waitmode, WAIT_TYPE_ANY,
					   timeout);

		IF_ERR(status)
		{
			return status;
		}
	} while (1);
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
	assert(mutex->owner == curthread());

	// reset to non-owned
	struct kthread *desired = curthread();
	if (likely(__atomic_compare_exchange_n(&mutex->owner, &desired, NULL, 0,
					       __ATOMIC_ACQ_REL,
					       __ATOMIC_RELAXED))) {
		event_alarm(&mutex->event);
		return;
	}

	panic("try to unlock mutex not owned by curthread");
}
