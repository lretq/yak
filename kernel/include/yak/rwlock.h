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
	uint32_t state;
	unsigned int exclusive_count;
};

void rwlock_init(struct rwlock *rwlock);

status_t rwlock_acquire_shared(struct rwlock *rwlock, nstime_t timeout);
void rwlock_release_shared(struct rwlock *rwlock);

status_t rwlock_acquire_exclusive(struct rwlock *rwlock, nstime_t timeout);
void rwlock_release_exclusive(struct rwlock *rwlock);

#ifdef __cplusplus
}
#endif
