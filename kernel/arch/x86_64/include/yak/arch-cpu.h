#pragma once

#include <stdint.h>

static inline int interrupt_state()
{
	uint64_t fq;
	asm volatile( //
		"pushfq\n\t"
		"pop %0"
		//
		: "=rm"(fq)::"memory");
	return fq & (1 << 9);
}

static inline int disable_interrupts()
{
	int state = interrupt_state();
	asm volatile("cli");
	return state;
}

static inline int enable_interrups()
{
	int state = interrupt_state();
	asm volatile("sti");
	return state;
}

[[gnu::noreturn]]
static inline void hcf()
{
	for (;;) {
		asm volatile("cli; hlt");
	}
	__builtin_unreachable();
}
