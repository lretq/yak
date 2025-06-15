#include <yak/ipl.h>
#include <yak/arch-cpu.h>
#include <yak/status.h>
#include <uacpi/status.h>
#include <uacpi/sleep.h>
#include <yak/log.h>

status_t acpi_shutdown()
{
	pr_info("acpi shutdown requested\n");

	uacpi_status ret = uacpi_prepare_for_sleep_state(UACPI_SLEEP_STATE_S5);
	if (uacpi_unlikely_error(ret)) {
		pr_error("failed to prepare for sleep: %s",
			 uacpi_status_to_string(ret));
		return YAK_IO;
	}

	disable_interrupts();

	ret = uacpi_enter_sleep_state(UACPI_SLEEP_STATE_S5);
	if (uacpi_unlikely_error(ret)) {
		pr_error("failed to enter sleep: %s",
			 uacpi_status_to_string(ret));
		return YAK_IO;
	}

	__builtin_unreachable();
	return YAK_SUCCESS;
}
