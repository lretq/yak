#define pr_fmt(fmt) "uacpi: " fmt

#include <stddef.h>
#include <uacpi/kernel_api.h>
#include <yak/log.h>
#include <yak/status.h>
#include <yak/vm/map.h>
#include <yak/vmflags.h>
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
	paddr_t rounded_addr = ALIGN_DOWN(addr, PAGE_SIZE);
	size_t offset = addr - rounded_addr;
	len = ALIGN_UP(len + offset, PAGE_SIZE);

	vaddr_t mapped_addr;
	status_t status = vm_map_mmio(kmap(), rounded_addr, len, VM_RW,
				      VM_CACHE_DEFAULT, &mapped_addr);
	IF_ERR(status)
	{
		pr_error("error while mapping 0x%lx: %s\n", addr,
			 status_str(status));
		return NULL;
	}

	return (void *)(mapped_addr + offset);
}

void uacpi_kernel_unmap(void *addr, uacpi_size len)
{
	(void)len;
	vaddr_t rounded_addr = ALIGN_DOWN((vaddr_t)addr, PAGE_SIZE);
	// uacpi shouldn't give us a non-existing address
	EXPECT(vm_unmap(kmap(), rounded_addr));
}

void uacpi_kernel_log(uacpi_log_level loglevel, const uacpi_char *logmsg)
{
	size_t translated_level = 6 - loglevel;
	printk(translated_level, "%s", logmsg);
}
