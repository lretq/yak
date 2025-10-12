#pragma once

void fpu_init();
void fpu_ap_init();
void *fpu_alloc();
void fpu_free(void *ptr);
void fpu_save(void *ptr);
void fpu_restore(void *ptr);
