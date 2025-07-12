#include <yak/spinlock.h>

#ifdef SPINLOCK_DEBUG
// #define SPINLOCK_DEBUG_OWNER
// #define SPINLOCK_DEBUG_
void spinlock_lock_noipl(struct spinlock *lock)
{
	while (!spinlock_trylock(lock)) {
		busyloop_hint();
	}
#ifdef SPINLOCK_DEBUG_OWNER
	lock->owner = curthread();
#endif
}

void spinlock_unlock_noipl(struct spinlock *lock)
{
#ifdef SPINLOCK_DEBUG_OWNER
	lock->owner = NULL;
#endif
	__atomic_store_n(&lock->state, SPINLOCK_UNLOCKED, __ATOMIC_RELEASE);
}
#endif
