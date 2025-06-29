#include <cstddef>
#include <yak/heap.h>
#include <yak/panic.h>
#include <yak/macro.h>

extern "C" {
void __cxa_pure_virtual()
{
	panic("__cxa_pure_virtual");
}

void __cxa_deleted_virtual()
{
	panic("__cxa_deleted_virtual");
}

void __cxa_atexit()
{
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

constexpr size_t HEADER_SIZE =
	ALIGN_UP(sizeof(size_t), alignof(std::max_align_t));

void *operator new[](size_t size)
{
	size_t real_size = HEADER_SIZE + size;
	size_t *ptr = (size_t *)kmalloc(real_size);
	if (ptr == nullptr)
		panic("new[] oom");
	*ptr = size;
	return (void *)((uintptr_t)ptr + HEADER_SIZE);
}

void operator delete[](void *ptr)
{
	if (!ptr)
		return;

	size_t *szptr = (size_t *)((uintptr_t)ptr - HEADER_SIZE);
	kfree(szptr, *szptr);
}
