#pragma once

#include <yak/ipl.h>

struct spinlock {
	int state;
};

#define SPINLOCK_INITIALIZER() {0}

#define spinlock_init(spinlock)        \
	do {                           \
		(spinlock)->state = 0; \
	} while (0)

#ifdef x86_64
#define busyloop_hint() asm volatile("pause" ::: "memory");
#else
#define busyloop_hint()
#endif

static inline int spinlock_trylock(struct spinlock *lock)
{
	int unlocked = 0;
	return __atomic_compare_exchange_n(&lock->state, &unlocked, 1, 0,
					   __ATOMIC_ACQ_REL, __ATOMIC_RELAXED);
}

static inline void spinlock_lock_noipl(struct spinlock *lock)
{
	while (!spinlock_trylock(lock)) {
		busyloop_hint();
	}
}

static inline void spinlock_unlock_noipl(struct spinlock *lock)
{
	__atomic_store_n(&lock->state, 0, __ATOMIC_RELEASE);
}

static inline ipl_t spinlock_lock(struct spinlock *lock)
{
	ipl_t ipl = ripl(IPL_DPC);
	spinlock_lock_noipl(lock);
	return ipl;
}

static inline void spinlock_unlock(struct spinlock *lock, ipl_t ipl)
{
	spinlock_unlock_noipl(lock);
	xipl(ipl);
}
