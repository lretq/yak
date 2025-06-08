#pragma once

#include <yak/arch-cpu.h>
#include <yak/ipl.h>

enum {
	SPINLOCK_UNLOCKED = 0,
	SPINLOCK_LOCKED = 1,
};

struct spinlock {
	int state;
};

#define SPINLOCK_INITIALIZER() { .state = SPINLOCK_UNLOCKED }

#define spinlock_init(spinlock)                        \
	do {                                           \
		(spinlock)->state = SPINLOCK_UNLOCKED; \
	} while (0)

#ifdef x86_64
#define busyloop_hint() asm volatile("pause" ::: "memory");
#else
#define busyloop_hint()
#endif

static inline int spinlock_trylock(struct spinlock *lock)
{
	int unlocked = SPINLOCK_UNLOCKED;
	return __atomic_compare_exchange_n(&lock->state, &unlocked,
					   SPINLOCK_LOCKED, 0, __ATOMIC_ACQ_REL,
					   __ATOMIC_RELAXED);
}

static inline void spinlock_lock_noipl(struct spinlock *lock)
{
	while (!spinlock_trylock(lock)) {
		busyloop_hint();
	}
}

static inline void spinlock_unlock_noipl(struct spinlock *lock)
{
	__atomic_store_n(&lock->state, SPINLOCK_UNLOCKED, __ATOMIC_RELEASE);
}

static inline ipl_t spinlock_lock(struct spinlock *lock)
{
	ipl_t ipl = ripl(IPL_DPC);
	spinlock_lock_noipl(lock);
	return ipl;
}

static inline int spinlock_lock_interrupts(struct spinlock *lock)
{
	int state = disable_interrupts();
	spinlock_lock_noipl(lock);
	return state;
}

static inline void spinlock_unlock_interrupts(struct spinlock *lock, int state)
{
	spinlock_unlock_noipl(lock);
	if (state)
		enable_interrupts();
}

static inline void spinlock_unlock(struct spinlock *lock, ipl_t ipl)
{
	spinlock_unlock_noipl(lock);
	xipl(ipl);
}

static inline int spinlock_held(struct spinlock *lock)
{
	return __atomic_load_n(&lock->state, __ATOMIC_RELAXED) ==
	       SPINLOCK_LOCKED;
}
