#pragma once

/*
 * This implementation was based on my idea of how the guard api from linux works 
*/

#define PASTE(a, b) a##b
#define EXPAND_AND_PASTE(a, b) PASTE(a, b)
#define EXPAND(a) a

#define DEFINE_CLEANUP_CLASS(name, members, fn_body, init_body, init_args...) \
	struct cleanup_##name members;                                        \
	static inline void cleanup_##name##_destroy_fn(                       \
		[[maybe_unused]] struct cleanup_##name *ctx) fn_body;         \
	static inline struct cleanup_##name cleanup_##name##_init_fn(         \
		init_args) init_body

#define RET(clazz, ...)                  \
	return (struct cleanup_##clazz){ \
		__VA_ARGS__,             \
	};

#define GUARD_INTERNAL(clazz, var)                     \
	[[gnu::cleanup(cleanup_##clazz##_destroy_fn)]] \
	struct cleanup_##clazz var = cleanup_##clazz##_init_fn

#define guard(clazz) \
	GUARD_INTERNAL(clazz, EXPAND_AND_PASTE(__cleanup_##clazz, __COUNTER__))
