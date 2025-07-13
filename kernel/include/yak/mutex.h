#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <yak/types.h>
#include <yak/object.h>
#include <yak/status.h>
#include <yak/kevent.h>

struct kmutex {
	struct kevent event;
#ifdef CONFIG_DEBUG
	// for debugging purposes
	const char *name;
#endif
	struct kthread *owner;
};

void kmutex_init(struct kmutex *mutex, const char *name);
status_t kmutex_acquire(struct kmutex *mutex, nstime_t timeout);
status_t kmutex_acquire_polling(struct kmutex *mutex, nstime_t timeout);
void kmutex_release(struct kmutex *mutex);

#ifdef __cplusplus
}
#endif
