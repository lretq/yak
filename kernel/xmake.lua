-- stylua: ignore start

target("yak.headers")
	set_kind("headeronly")
	add_includedirs(
		"$(projectdir)/kernel/include",
		"$(projectdir)/kernel/arch/$(arch)",
		{ public = true }
	)
	add_deps("freestnd-c-hdrs")

target("yak.deps")
	set_default(false)
	set_kind("phony")
	add_deps("yak.headers", "nanoprintf", "cc-runtime")

rule("kernel")
	on_load(function(target)
		target:add("deps", "yak.deps")
		target:add("defines","ARCH=$(arch)", "$(arch)")
	end)

includes("arch/x86_64/xmake.lua",
	"arch/riscv64/xmake.lua",
	"arch/generic-limine/xmake.lua")

target("yak.elf")
	set_default(false)
	set_kind("binary")
	add_rules("kernel")
	add_deps("yak.arch.$(arch)")
	add_files("src/**.c")

	add_ldflags("-T$(projectdir)/kernel/arch/$(arch)/$(port)/linker.lds", { force = true })

	on_run(function() end)
