#pragma once

#include <yak/object.h>

struct kmutex {
	struct kobject_header header;
	struct kthread *owner;
};

void kmutex_init(struct kmutex *mutex);
void kmutex_acquire(struct kmutex *mutex);
void kmutex_release(struct kmutex *mutex);
