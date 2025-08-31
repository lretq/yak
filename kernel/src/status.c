#include <yak/status.h>
#include <yak/macro.h>
#include <yak-abi/errno.h>

static const char *status_names[] = {
	"success",
	"no entry",
	"NULL page ref",
	"not implemented",
	"busy",
	"out of memory",
	"timeout",
	"i/o error",
	"invalid arguments",
	"unknown filesystem",
	"expected directory",
	"exists already",
	"no space left",
	"end of file",
};

const char *status_str(status_t status)
{
	if (status >= elementsof(status_names) || status < 0) {
		return "unknown";
	}
	return status_names[status];
}

int status_errno(status_t status)
{
	switch (status) {
	case YAK_SUCCESS:
		return 0;
	case YAK_NOENT:
		return ENOENT;
	case YAK_NULL_DEREF:
		return EFAULT;
	case YAK_NOT_IMPLEMENTED:
		return ENOSYS;
	case YAK_BUSY:
		return EBUSY;
	case YAK_OOM:
		return ENOMEM;
	case YAK_TIMEOUT:
		return ETIMEDOUT;
	case YAK_IO:
		return EIO;
	case YAK_INVALID_ARGS:
		return EINVAL;
	case YAK_UNKNOWN_FS:
		return ENODEV;
	case YAK_NODIR:
		return ENOTDIR;
	case YAK_EXISTS:
		return EEXIST;
	case YAK_NOSPACE:
		return ENOSPC;
	case YAK_EOF:
		return 0; // may be wrong?
	default:
		return EINVAL; // fallback
	}
}
