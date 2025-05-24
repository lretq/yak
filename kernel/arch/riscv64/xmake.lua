-- stylua: ignore start

target("yak.arch.riscv64")
	set_enabled(is_arch("riscv64"))
	set_kind("static")
	add_rules("kernel", "limine")
	add_files("**.c")
	add_files("**.S")
