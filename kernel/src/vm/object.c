#include <yak/vm/map.h>

void vm_object_common_init(struct vm_object *obj, struct vm_pagerops *pgops)
{
	kmutex_init(&obj->obj_lock, "obj_lock");
	obj->pg_ops = pgops;
	obj->refcnt = 1;
}
