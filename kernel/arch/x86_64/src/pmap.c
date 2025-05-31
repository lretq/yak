#include <yak/vm/pmap.h>
#include "asm.h"

void pmap_activate(struct pmap *pmap)
{
	write_cr3(pmap->top_level);
}
