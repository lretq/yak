-- stylua: ignore start

target("uacpi")
	set_kind("static")
	add_deps("yak.headers")
	add_includedirs("uacpi/include", { public = true })
	add_defines("UACPI_NATIVE_ALLOC_ZEROED=1", "UACPI_SIZED_FREES=1", { public = true })
	add_files(
		'uacpi/source/tables.c',
		'uacpi/source/types.c',
    		'uacpi/source/uacpi.c',
    		'uacpi/source/utilities.c',
    		'uacpi/source/interpreter.c',
    		'uacpi/source/opcodes.c',
    		'uacpi/source/namespace.c',
    		'uacpi/source/stdlib.c',
    		'uacpi/source/shareable.c',
    		'uacpi/source/opregion.c',
    		'uacpi/source/default_handlers.c',
    		'uacpi/source/io.c',
    		'uacpi/source/notify.c',
    		'uacpi/source/sleep.c',
    		'uacpi/source/registers.c',
    		'uacpi/source/resources.c',
    		'uacpi/source/event.c',
    		'uacpi/source/mutex.c',
    		'uacpi/source/osi.c'
	)
