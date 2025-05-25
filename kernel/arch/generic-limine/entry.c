#include <limine.h>

extern void plat_boot();

// kernel entrypoint for every limine-based arch
void _start()
{
	plat_boot();
}
