#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <yak/hint.h>
#include <yak/panic.h>

typedef enum status {
	YAK_SUCCESS = 0,
	YAK_NOENT,
	YAK_NULL_DEREF,
	YAK_NOT_IMPLEMENTED,
	YAK_BUSY,
	YAK_OOM,
	YAK_TIMEOUT,
	YAK_IO,
} status_t;

#define IS_OK(x) (likely((x) == YAK_SUCCESS))
#define IS_ERR(x) (unlikely((x) != YAK_SUCCESS))

#define IF_OK(expr) if (IS_OK((expr)))
#define IF_ERR(expr) if (IS_ERR((expr)))

#define EXPECT(expr)                                                   \
	IF_ERR(expr)                                                   \
	{                                                              \
		panic("%s:%d\\%s: unexpected failure: %s\n", __FILE__, \
		      __LINE__, __func__, #expr);                      \
	}

const char *status_str(unsigned int status);

#ifdef __cplusplus
}
#endif
