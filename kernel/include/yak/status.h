#pragma once

#include <yak/hint.h>
#include <yak/panic.h>

typedef enum status {
	YAK_SUCCESS = 0,
	YAK_NOENT,
	YAK_NOT_IMPLEMENTED,
	YAK_BUSY,
	YAK_OOM,
	YAK_TIMEOUT
} status_t;

#define IS_OK(x) (likely((x) == YAK_SUCCESS))
#define IS_ERR(x) (unlikely((x) != YAK_SUCCESS))

#define IF_OK(expr) if (IS_OK((expr)))
#define IF_ERR(expr) if (IS_ERR((expr)))

#define EXPECT(expr)                                                         \
	IF_ERR(expr)                                                         \
	{                                                                    \
		panic("%s:%d\\%s: unexpected failure\n", __FILE__, __LINE__, \
		      __func__);                                             \
	}

const char *status_str(unsigned int status);
