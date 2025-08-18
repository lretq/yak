#pragma once

/*
 * This implementation was based on my idea of how the guard api from linux works 
*/

#ifdef __cplusplus
#error "Use C++ RAII implementations"
#endif

#include <yak/mutex.h>
#include <yak/log.h>

#define cleanup_class(name, members, fn_body)   \
	typedef struct cleanup_class_name {     \
	} cleanup_class_##name##_t;             \
	struct cleanup_##name members;          \
	static inline void cleanup_##name##_fn( \
		[[maybe_unused]] struct cleanup_##name *ctx) fn_body

cleanup_class(mutex, { struct kmutex *mutex; }, { kmutex_release(ctx->mutex); });

static inline struct cleanup_mutex cleanup_create_mutex(struct kmutex *mutex)
{
	EXPECT(kmutex_acquire(mutex, TIMEOUT_INFINITE));
	return (struct cleanup_mutex){ .mutex = mutex };
}

#define PASTE(a, b) a##b
#define EXPAND_AND_PASTE(a, b) PASTE(a, b)

#define guard_internal(clazz, tmp, var)        \
	cleanup_class_##clazz##_t tmp;         \
	[[gnu::cleanup(cleanup_##clazz##_fn)]] \
	struct cleanup_##clazz var =           \
		_Generic(tmp, cleanup_class_mutex_t: cleanup_create_mutex)

#define guard(clazz)                                                      \
	guard_internal(clazz, EXPAND_AND_PASTE(__guard_sel, __COUNTER__), \
		       EXPAND_AND_PASTE(__cleanup_##clazz##_var_,         \
					__COUNTER__))
