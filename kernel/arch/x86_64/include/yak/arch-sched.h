#pragma once

#include <stdint.h>

struct md_pcb {
	uint64_t rbx, rsp, rbp, r12, r13, r14, r15;
};
