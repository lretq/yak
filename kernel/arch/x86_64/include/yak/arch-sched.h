#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

struct md_pcb {
	uint64_t rbx, rsp, rbp, r12, r13, r14, r15;

	uint64_t fsbase, gsbase;
};

#ifdef __cplusplus
}
#endif
