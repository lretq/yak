#include <cstddef>
#include <yak/heap.h>
#include <yak/panic.h>

extern "C" {
void __cxa_pure_virtual()
{
	panic("__cxa_pure_virtual");
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
