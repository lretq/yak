-- stylua: ignore start

option("ubsan")
	set_category("kernel sanitizers")
	set_default(false)
	set_description("Enable kernel ubsan")
	add_cflags("-fsanitize=undefined")
	add_defines("CONFIG_UBSAN=1")
option_end()

target("yak.headers")
	set_kind("headeronly")
	add_includedirs(
		"$(projectdir)/kernel/include",
		"$(projectdir)/kernel/arch/$(arch)/include",
		{ public = true }
	)
	add_deps("freestnd-c-hdrs")

target("yak.deps")
	set_default(false)
	set_kind("phony")
	add_deps("yak.headers", "nanoprintf", "cc-runtime", "uacpi", "flanterm", "pairing-heap")

rule("kernel")
	on_load(function(target)
		target:add("options", "ubsan")
		target:add("deps", "yak.deps")
		target:add("defines","ARCH=\"$(arch)\"", "$(arch)")
	end)
	on_config(function(target)
		if is_mode("debug") then
			target:add("defines", "CONFIG_DEBUG=1")
		end
	end)

includes("arch/x86_64/xmake.lua",
	"arch/riscv64/xmake.lua",
	"arch/generic-limine/xmake.lua",
	"io/xmake.lua")

target("yak.elf")
	set_default(false)
	set_kind("binary")
	add_rules("kernel")
	add_deps("yak.arch.$(arch)")
	add_deps("yak.io.builtin")
	add_files("src/**.c")

	add_ldflags("-T$(projectdir)/kernel/arch/$(arch)/$(port)/linker.lds", { force = true })

	on_run(function() end)
