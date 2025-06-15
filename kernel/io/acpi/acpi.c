#include <uacpi/uacpi.h>
#include <uacpi/event.h>
#include <yak/log.h>
#include <yak/status.h>
#include <yak/io/acpi.h>
#include <yak/io/acpi/event.h>

status_t acpi_init()
{
	uacpi_status uret = uacpi_initialize(0);
	if (uacpi_unlikely_error(uret)) {
		pr_error("uacpi_initialize error: %s\n",
			 uacpi_status_to_string(uret));
		return YAK_NOENT;
	}

	uret = uacpi_namespace_load();
	if (uacpi_unlikely_error(uret)) {
		pr_error("uacpi_namespace_load error: %s\n",
			 uacpi_status_to_string(uret));
		return YAK_NOENT;
	}

	uret = uacpi_namespace_initialize();
	if (uacpi_unlikely_error(uret)) {
		pr_error("uacpi_namespace_initialize: %s\n",
			 uacpi_status_to_string(uret));
		return YAK_NOENT;
	}

	acpi_init_events();

	uret = uacpi_finalize_gpe_initialization();
	if (uacpi_unlikely_error(uret)) {
		pr_error("uacpi_finalize_gpe_initialization: %s\n",
			 uacpi_status_to_string(uret));
		return YAK_NOENT;
	}

	pr_info("acpi: init done\n");

	return YAK_SUCCESS;
}
