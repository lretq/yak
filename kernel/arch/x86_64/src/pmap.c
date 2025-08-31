#include <yak/vm/pmap.h>
#include "asm.h"

void pmap_activate(struct pmap *pmap)
{
	if (read_cr3() == pmap->top_level)
		return;
	write_cr3(pmap->top_level);
}
