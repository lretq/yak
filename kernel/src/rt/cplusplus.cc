#include "yak/hint.h"
#include "yak/log.h"
#include <cstddef>
#include <yak/heap.h>
#include <yak/panic.h>
#include <yak/macro.h>
#include <assert.h>

extern "C" {
void *__dso_handle = &__dso_handle;

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

void operator delete(void *ptr) noexcept
{
	kfree(ptr, 0);
}

void operator delete(void *ptr, [[maybe_unused]] size_t size) noexcept
{
	kfree(ptr, 0);
}

void *operator new[](size_t size)
{
	return kmalloc(size);
}

void operator delete[](void *ptr)
{
	kfree(ptr, 0);
}
