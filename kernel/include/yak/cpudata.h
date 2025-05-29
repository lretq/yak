#pragma once

#include <stddef.h>
#include <yak/ipl.h>
#include <yak/percpu.h>
#include <yak/arch-cpudata.h>

struct cpu {
	struct cpu_md md;
	struct cpu *self;

	size_t cpu_id;
};
