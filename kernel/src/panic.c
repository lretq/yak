#include <yak/panic.h>
#include <yak/log.h>
#include <yak/arch-cpu.h>
#include <yak/hint.h>
#include <stdarg.h>

static int in_panic;

int is_in_panic()
{
	return __atomic_load_n(&in_panic, __ATOMIC_SEQ_CST);
}

__no_san __noreturn void panic(const char *fmt, ...)
{
	va_list args;
	va_start(args, fmt);

	__atomic_store_n(&in_panic, 1, __ATOMIC_SEQ_CST);

	vprintk(LOG_FAIL, fmt, args);

	va_end(args);

	hcf();
}
