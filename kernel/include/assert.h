#pragma once

#include <yak/hint.h>

[[gnu::noreturn]]
void __assert_fail(const char *assertion, const char *file, unsigned int line,
		   const char *function);

#undef assert
#define assert(expr)              \
	((void)(likely((expr)) || \
		(__assert_fail(#expr, __FILE__, __LINE__, __func__), 0)))
