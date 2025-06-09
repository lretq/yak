-- stylua: ignore start

target("flanterm")
	set_kind("static")
	add_deps("cc-runtime")
	add_defines("FLANTERM_FB_DISABLE_BUMP_ALLOC=1")
	add_includedirs("flanterm/src", { public = true })
	add_files("flanterm/src/**.c")
