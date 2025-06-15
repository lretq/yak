#include <yak/sched.h>
#include <yak/io/acpi.h>

void io_init()
{
	extern void plat_pci_init();
	plat_pci_init();

	acpi_init();
}
