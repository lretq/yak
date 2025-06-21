#include <cstddef>
#include <yak/heap.h>
#include <yak/panic.h>

extern "C" {
void __cxa_pure_virtual()
{
	panic("__cxa_pure_virtual");
}

void __cxa_deleted_virtual()
{
	panic("__cxa_deleted_virtual");
}
}

void *operator new(std::size_t size)
{
	return kmalloc(size);
}

void operator delete(void *ptr, size_t size) noexcept
{
	kfree(ptr, size);
}

void *operator new[](size_t size)
{
	size = sizeof(size_t) + size;
	size_t *ptr = (size_t *)kmalloc(size);
	*ptr = size;
	return ++ptr;
}

void operator delete[](void *ptr)
{
	if (!ptr)
		return;
	size_t *szptr = (size_t *)ptr;
	--szptr;
	kfree(szptr, *szptr);
}
