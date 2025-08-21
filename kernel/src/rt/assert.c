#include <yak/panic.h>
#include <assert.h>

void __assert_fail(const char *assertion, const char *file, unsigned int line,
		   const char *function)
{
	panic("Assertion failure at %s:%d in function %s: %s\n", file, line,
	      function, assertion);
}
