target_compile_definitions(kernel
	PUBLIC
		LIMINE_API_REVISION=3
)

yak_add_sources(
	fb.c
	entry.c
	fireworks.c
)
