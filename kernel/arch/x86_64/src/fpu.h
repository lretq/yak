#pragma once

#include <stddef.h>

void fpu_init();
void fpu_ap_init();
void *fpu_alloc();
size_t fpu_statesize();
void fpu_free(void *ptr);
void fpu_save(void *ptr);
void fpu_restore(void *ptr);
