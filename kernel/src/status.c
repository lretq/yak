#include <yak/status.h>
#include <yak/macro.h>

static const char *status_names[] = {
	"success",
	"no entry",
};

const char *status_str(unsigned int status)
{
	if (status >= elementsof(status_names) || status < 0) {
		return "unknown";
	}
	return status_names[status];
}
