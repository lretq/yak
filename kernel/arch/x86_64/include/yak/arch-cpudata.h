#pragma once

#include <stdint.h>

struct cpu_md {
	uint32_t apic_id;
	uint64_t apic_ticks_per_ms;
};

extern char __kernel_percpu_start[];

#define curcpu() (*(__seg_gs struct cpu *)__kernel_percpu_start)
#define curcpu_ptr() (curcpu().self)
