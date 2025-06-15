#pragma once

#ifdef __cplusplus
extern "C" {
#endif

extern char __kernel_limine_start[];
extern char __kernel_limine_end[];

extern char __kernel_text_start[];
extern char __kernel_text_end[];

extern char __kernel_rodata_start[];
extern char __kernel_rodata_end[];

extern char __kernel_data_start[];
extern char __kernel_data_end[];

extern char __kernel_percpu_start[];
extern char __kernel_percpu_end[];

#ifdef __cplusplus
}
#endif
