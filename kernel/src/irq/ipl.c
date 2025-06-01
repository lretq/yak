#include <yak/ipl.h>
#include <yak/cpudata.h>
#include <yak/arch-cpu.h>
#include <assert.h>

ipl_t ripl(ipl_t ipl)
{
	ipl_t old = curipl();
	assert(ipl >= old);
	setipl(ipl);
	return old;
}

void xipl(ipl_t ipl)
{
	ipl_t old = curipl();
	assert(ipl <= old);
	setipl(ipl);
}
