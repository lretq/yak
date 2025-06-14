#include <assert.h>
#include <yak/object.h>
#include <yak/sched.h>
#include <yak/mutex.h>
#include <yak/cpudata.h>

void kmutex_init(struct kmutex *mutex)
{
	// initiate as signalled
	kobject_init(&mutex->header, 1);
}

status_t kmutex_acquire(struct kmutex *mutex, nstime_t timeout)
{
	assert(mutex->owner != curthread());
	status_t status = sched_wait_single(mutex, WAIT_MODE_BLOCK,
					    WAIT_TYPE_ANY, timeout);
	mutex->owner = curthread();
	return status;
}

status_t kmutex_acquire_polling(struct kmutex *mutex, nstime_t timeout)
{
	assert(mutex->owner != curthread());
	status_t status = sched_wait_single(mutex, WAIT_MODE_POLL,
					    WAIT_TYPE_ANY, timeout);
	mutex->owner = curthread();
	return status;
}

void kmutex_release(struct kmutex *mutex)
{
	ipl_t ipl = spinlock_lock(&mutex->header.obj_lock);
	mutex->owner = NULL;
	if (mutex->header.waitcount) {
		kobject_signal_locked(&mutex->header, 0);
	}
	// reset to non-owned
	mutex->header.signalstate = 1;
	spinlock_unlock(&mutex->header.obj_lock, ipl);
}
