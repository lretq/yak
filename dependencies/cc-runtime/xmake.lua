-- stylua: ignore start

target("cc-runtime")
	set_kind("static")
	add_deps("freestnd-c-hdrs")
	add_files("cc-runtime/src/cc-runtime.c")
