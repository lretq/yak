#include <yak/status.h>
#include <yak/macro.h>

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
};

const char *status_str(unsigned int status)
{
	if (status >= elementsof(status_names) || status < 0) {
		return "unknown";
	}
	return status_names[status];
}
