#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <yak/types.h>
#include <yak/object.h>
#include <yak/status.h>

struct kmutex {
	struct kobject_header header;
	struct kthread *owner;
};

void kmutex_init(struct kmutex *mutex);
status_t kmutex_acquire(struct kmutex *mutex, nstime_t timeout);
status_t kmutex_acquire_polling(struct kmutex *mutex, nstime_t timeout);
void kmutex_release(struct kmutex *mutex);

#ifdef __cplusplus
}
#endif
