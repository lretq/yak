#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>

void heap_init();

void *kcalloc(size_t size);
void *kmalloc(size_t size);
void kfree(void *ptr, size_t size);

#ifdef __cplusplus
}
#endif
