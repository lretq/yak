#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <yak/object.h>
#include <yak/status.h>
#include <yak/types.h>
#include <yak/kevent.h>

struct rwlock {
	struct kevent event;
#ifdef CONFIG_DEBUG
	const char *name;
#endif
	struct kthread *exclusive_owner;
	uint32_t state;
	unsigned int exclusive_count;
};

void rwlock_init(struct rwlock *rwlock, const char *name);

status_t rwlock_acquire_shared(struct rwlock *rwlock, nstime_t timeout);
void rwlock_release_shared(struct rwlock *rwlock);

status_t rwlock_acquire_exclusive(struct rwlock *rwlock, nstime_t timeout);
void rwlock_release_exclusive(struct rwlock *rwlock);

void rwlock_upgrade_to_exclusive(struct rwlock *rwlock);

static inline struct kthread *rwlock_fetch_owner(struct rwlock *rwlock)
{
	return __atomic_load_n(&rwlock->exclusive_owner, __ATOMIC_ACQUIRE);
}

#ifdef __cplusplus
}
#endif
