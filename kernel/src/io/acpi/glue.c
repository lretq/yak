#define pr_fmt(fmt) "uacpi: " fmt

#include <stddef.h>
#include <uacpi/kernel_api.h>
#include <yak/log.h>
#include <yak/mutex.h>
#include <yak/cpudata.h>
#include <yak/status.h>
#include <yak/vm/map.h>
#include <yak/vmflags.h>
#include <yak/heap.h>
#include <yak/timer.h>
#include <yak/macro.h>
#include <yak/arch-mm.h>
#include <yak/types.h>

extern paddr_t plat_get_rsdp();

uacpi_status uacpi_kernel_get_rsdp(uacpi_phys_addr *out_rsdp_address)
{
	paddr_t addr = plat_get_rsdp();
	if (addr == 0) {
		*out_rsdp_address = 0;
		return UACPI_STATUS_NOT_FOUND;
	}
	*out_rsdp_address = addr;
	return UACPI_STATUS_OK;
}

void *uacpi_kernel_map(uacpi_phys_addr addr, uacpi_size len)
{
	vaddr_t mapped_addr;
	status_t status = vm_map_mmio(kmap(), addr, len, VM_RW,
				      VM_CACHE_DEFAULT, &mapped_addr);
	IF_ERR(status)
	{
		pr_error("error while mapping 0x%lx: %s\n", addr,
			 status_str(status));
		return NULL;
	}

	return (void *)mapped_addr;
}

void uacpi_kernel_unmap(void *addr, uacpi_size len)
{
	(void)len;
	EXPECT(vm_unmap_mmio(kmap(), (vaddr_t)addr));
}

void uacpi_kernel_log(uacpi_log_level loglevel, const uacpi_char *logmsg)
{
	size_t translated_level = 6 - loglevel;
	printk(translated_level, "uacpi: %s", logmsg);
}

#ifdef x86_64
uacpi_status uacpi_kernel_io_map(uacpi_io_addr base,
				 [[maybe_unused]] uacpi_size len,
				 uacpi_handle *out_handle)
{
	*out_handle = (uacpi_handle)base;
	return UACPI_STATUS_OK;
}

void uacpi_kernel_io_unmap([[maybe_unused]] uacpi_handle handle)
{
}

#include "../arch/x86_64/src/asm.h"

uacpi_status uacpi_kernel_io_read8(uacpi_handle handle, uacpi_size offset,
				   uacpi_u8 *out_value)
{
	uacpi_io_addr addr = (uacpi_io_addr)handle + offset;
	*out_value = inb(addr);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read16(uacpi_handle handle, uacpi_size offset,
				    uacpi_u16 *out_value)
{
	uacpi_io_addr addr = (uacpi_io_addr)handle + offset;
	*out_value = inw(addr);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_read32(uacpi_handle handle, uacpi_size offset,
				    uacpi_u32 *out_value)
{
	uacpi_io_addr addr = (uacpi_io_addr)handle + offset;
	*out_value = inl(addr);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write8(uacpi_handle handle, uacpi_size offset,
				    uacpi_u8 in_value)
{
	uacpi_io_addr addr = (uacpi_io_addr)handle + offset;
	outb(addr, in_value);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write16(uacpi_handle handle, uacpi_size offset,
				     uacpi_u16 in_value)
{
	uacpi_io_addr addr = (uacpi_io_addr)handle + offset;
	outw(addr, in_value);
	return UACPI_STATUS_OK;
}

uacpi_status uacpi_kernel_io_write32(uacpi_handle handle, uacpi_size offset,
				     uacpi_u32 in_value)
{
	uacpi_io_addr addr = (uacpi_io_addr)handle + offset;
	outl(addr, in_value);
	return UACPI_STATUS_OK;
}
#endif

void *uacpi_kernel_alloc(uacpi_size size)
{
	return kmalloc(size);
}

void *uacpi_kernel_alloc_zeroed(uacpi_size size)
{
	return kcalloc(size);
}

void uacpi_kernel_free(void *mem, uacpi_size size_hint)
{
	kfree(mem, size_hint);
}

uacpi_u64 uacpi_kernel_get_nanoseconds_since_boot(void)
{
	return plat_getnanos();
}

void uacpi_kernel_stall(uacpi_u8 usec)
{
	// ns
	usec *= 1000;

	uint64_t deadline = plat_getnanos() + usec;
	while (plat_getnanos() < deadline) {
		busyloop_hint();
	}
}

void uacpi_kernel_sleep(uacpi_u64 msec)
{
	ksleep(msec * 1000 * 1000);
}

uacpi_handle uacpi_kernel_create_mutex(void)
{
	struct kmutex *mutex = kmalloc(sizeof(struct kmutex));
	kmutex_init(mutex);
	return mutex;
}

void uacpi_kernel_free_mutex(uacpi_handle handle)
{
	kfree(handle, sizeof(struct kmutex));
}

uacpi_status uacpi_kernel_acquire_mutex(uacpi_handle handle,
					uacpi_u16 ms_timeout);

void uacpi_kernel_release_mutex(uacpi_handle handle)
{
	kmutex_release(handle);
}

uacpi_handle uacpi_kernel_create_event(void);
void uacpi_kernel_free_event(uacpi_handle handle);

uacpi_bool uacpi_kernel_wait_for_event(uacpi_handle handle,
				       uacpi_u16 ms_timeout);

void uacpi_kernel_signal_event(uacpi_handle handle);

void uacpi_kernel_reset_event(uacpi_handle handle);

uacpi_thread_id uacpi_kernel_get_thread_id(void)
{
	return (void *)curthread();
}

#ifndef UACPI_BAREBONES_MODE
uacpi_status uacpi_kernel_handle_firmware_request(uacpi_firmware_request *req)
{
	panic("firmware request??");
}

uacpi_status uacpi_kernel_install_interrupt_handler(
	uacpi_u32 irq, uacpi_interrupt_handler handler, uacpi_handle ctx,
	uacpi_handle *out_irq_handle);

uacpi_status
uacpi_kernel_uninstall_interrupt_handler(uacpi_interrupt_handler handler,
					 uacpi_handle irq_handle);

uacpi_status uacpi_kernel_schedule_work(uacpi_work_type type,
					uacpi_work_handler handler,
					uacpi_handle ctx);

uacpi_status uacpi_kernel_wait_for_work_completion(void);
#endif

uacpi_handle uacpi_kernel_create_spinlock(void)
{
	return kmalloc(sizeof(struct spinlock));
}

void uacpi_kernel_free_spinlock(uacpi_handle handle)
{
	kfree(handle, sizeof(struct spinlock));
}

uacpi_cpu_flags uacpi_kernel_lock_spinlock(uacpi_handle handle)
{
	return spinlock_lock_interrupts(handle);
}

void uacpi_kernel_unlock_spinlock(uacpi_handle handle, uacpi_cpu_flags state)
{
	return spinlock_unlock_interrupts(handle, state);
}
