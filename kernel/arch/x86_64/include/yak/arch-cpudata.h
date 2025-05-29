#pragma once

struct cpu_md {};

extern char __kernel_percpu_start[];

#define curcpu() (*(__seg_gs struct cpu *)__kernel_percpu_start)
#define curcpu_ptr() (curcpu().self)
