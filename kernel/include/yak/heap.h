#pragma once

#include <stddef.h>

void heap_init();

void *kcalloc(size_t size);
void *kmalloc(size_t size);
void kfree(void *ptr, size_t size);
