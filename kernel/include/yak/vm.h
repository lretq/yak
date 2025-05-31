#pragma once

#include <stdint.h>
#include <yak/arch-mm.h>

static inline uintptr_t p2v(uintptr_t p)
{
	return p + HHDM_BASE;
}

static inline uintptr_t v2p(uintptr_t v)
{
	return v - HHDM_BASE;
}
