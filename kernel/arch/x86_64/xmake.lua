-- stylua: ignore start

target("yak.arch.x86_64")
	set_enabled(is_arch("x86_64"))
	set_kind("static")
	add_rules("kernel", "limine")
	add_files("src/**.c")
	add_files("src/**.cc")
	add_files("src/**.S")
