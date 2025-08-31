#pragma once

#include <stddef.h>

#define DECLARE_REFMAINT(type)           \
	void type##_ref(struct type *p); \
	void type##_deref(struct type *p);

#define GENERATE_REFMAINT_INTERNAL(fn_attr, field, type, destructor)           \
	[[gnu::used]]                                                          \
	fn_attr void type##_ref(struct type *p)                                \
	{                                                                      \
		__atomic_fetch_add(&p->field, 1, __ATOMIC_ACQUIRE);            \
	}                                                                      \
	[[gnu::used]]                                                          \
	fn_attr void type##_deref(struct type *p)                              \
	{                                                                      \
		if (__atomic_fetch_sub(&p->field, 1, __ATOMIC_ACQ_REL) == 1) { \
			destructor(p);                                         \
		}                                                              \
	}

#define GENERATE_REFMAINT(t, f, d) GENERATE_REFMAINT_INTERNAL(, f, t, d)

#define GENERATE_REFMAINT_INLINE(t, f, d) \
	GENERATE_REFMAINT_INTERNAL(static inline, f, t, d)

typedef size_t refcount_t;
