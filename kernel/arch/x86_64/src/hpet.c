#include <stdint.h>
#include <uacpi/acpi.h>
#include <uacpi/tables.h>
#include <yak/status.h>
#include <yak/vm/map.h>
#include <yak/types.h>

enum {
	HPET_REG_GENERAL_CAPABILITY = 0x00,
	HPET_REG_GENERAL_CONFIG = 0x10,
	HPET_REG_GENERAL_INT_STATUS = 0x20,
	HPET_REG_COUNTER_MAIN = 0xF0,
};

#define FEMTO 1000000000000000ULL

struct hpet {
	vaddr_t base;

	uint16_t min_clock_ticks;
	uint32_t period;
};

static struct hpet hpet;

static void hpet_write(const uint64_t reg, const uint64_t value)
{
	*(volatile uint64_t *)(hpet.base + reg) = value;
}

static uint64_t hpet_read(const uint64_t reg)
{
	return *(volatile uint64_t *)(hpet.base + reg);
}

uint64_t hpet_counter()
{
	return hpet_read(HPET_REG_COUNTER_MAIN);
}

void hpet_enable()
{
	hpet_write(HPET_REG_GENERAL_CONFIG,
		   hpet_read(HPET_REG_GENERAL_CONFIG) | 0b1);
}

void hpet_disable()
{
	hpet_write(HPET_REG_GENERAL_CONFIG,
		   hpet_read(HPET_REG_GENERAL_CONFIG) & ~(0b1));
}

uint64_t hpet_nanos()
{
	return (hpet_counter() * hpet.period) / 1000000;
}

static int hpet_probe()
{
	uacpi_table tbl;
	if (uacpi_table_find_by_signature("HPET", &tbl) != UACPI_STATUS_OK)
		return YAK_NOENT;
	uacpi_table_unref(&tbl);
	return YAK_SUCCESS;
}

status_t hpet_setup()
{
	IF_ERR(hpet_probe()) return YAK_NOENT;

	uacpi_table tbl;
	uacpi_table_find_by_signature("HPET", &tbl);
	struct acpi_hpet *hpet_table = tbl.ptr;

	EXPECT(vm_map_mmio(kmap(), hpet_table->address.address, PAGE_SIZE,
			   VM_RW, VM_CACHE_DISABLE, &hpet.base));

	hpet.min_clock_ticks = hpet_table->min_clock_tick;
	hpet.period = hpet_read(HPET_REG_GENERAL_CAPABILITY) >> 32;

	hpet_write(HPET_REG_COUNTER_MAIN, 0);
	hpet_enable();

	uacpi_table_unref(&tbl);

	return YAK_SUCCESS;
}

nstime_t plat_getnanos()
{
	return hpet_nanos();
}
