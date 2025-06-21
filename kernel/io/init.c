#include <yak/sched.h>
#include <yak/io/acpi.h>

extern void iotest_fn();

void io_init()
{
	extern void plat_pci_init();
	plat_pci_init();

	acpi_init();

	iotest_fn();
}
