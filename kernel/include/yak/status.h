#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <yak/hint.h>
#include <yak/panic.h>
#include <yak/macro.h>

typedef enum status {
	YAK_SUCCESS = 0,
	YAK_NOENT,
	YAK_NULL_DEREF,
	YAK_NOT_IMPLEMENTED,
	YAK_BUSY,
	YAK_OOM,
	YAK_TIMEOUT,
	YAK_IO,
	YAK_INVALID_ARGS,
	YAK_UNKNOWN_FS,
	YAK_NODIR,
	YAK_EXISTS,
	YAK_NOSPACE,
	YAK_EOF,
	YAK_MFILE, /* process has too many opened files */
} status_t;

#define IS_OK(x) (likely((x) == YAK_SUCCESS))
#define IS_ERR(x) (unlikely((x) != YAK_SUCCESS))

#define IF_OK(expr) if (IS_OK((expr)))
#define IF_ERR(expr) if (IS_ERR((expr)))

#define _RET_ERRNO_ON_ERR_INTERNAL(expr, resvar)     \
	do {                                         \
		status_t resvar = expr;              \
		IF_ERR(resvar)                       \
		{                                    \
			return status_errno(resvar); \
		}                                    \
	} while (0)

#define RET_ERRNO_ON_ERR(expr)           \
	_RET_ERRNO_ON_ERR_INTERNAL(expr, \
				   EXPAND_AND_PASTE(__autoret, __COUNTER__))

#define EXPECT(expr)                                                          \
	do {                                                                  \
		status_t tmp_res = expr;                                      \
		IF_ERR(tmp_res)                                               \
		{                                                             \
			panic("%s:%d %s: unexpected failure: %s\n", __FILE__, \
			      __LINE__, __func__, status_str(tmp_res));       \
		}                                                             \
	} while (0)

const char *status_str(status_t status);
int status_errno(status_t status);

#ifdef __cplusplus
}
#endif
