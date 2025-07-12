-- stylua: ignore start

option("ubsan")
	set_category("kernel sanitizers")
	set_default(false)
	set_description("Enable kernel ubsan")
	add_cxflags("-fsanitize=undefined")
	add_defines("CONFIG_UBSAN=1")
option_end()

option("profiler")
	set_category("kernel util")
	set_default(false)
	set_description("Enable kernel profiler support")
	add_cxflags("-finstrument-functions")
	add_defines("KERNEL_PROFILER=1")
option_end()

target("yak.headers")
	set_kind("headeronly")
	add_includedirs(
		"$(projectdir)/kernel/include",
		"$(projectdir)/kernel/arch/$(arch)/include",
		{ public = true }
	)
	add_deps("freestnd-c-hdrs")
	add_deps("freestnd-cxx-hdrs")

target("yak.deps")
	set_default(false)
	set_kind("phony")
	add_deps("yak.headers", "nanoprintf", "cc-runtime", "uacpi", "flanterm", "pairing-heap")

rule("kernel")
	on_load(function(target)
		target:add("options", "ubsan", "profiler")
		target:add("deps", "yak.deps")
		target:add("defines","ARCH=\"$(arch)\"", "$(arch)")
		target:add("defines", "__YAK_PRIVATE__")
		target:add("cxxflags", "-fno-threadsafe-statics")
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
	add_files("src/**.cc")

	if not has_config("profiler") then
		remove_files("src/rt/profiler.c")
	end

	add_ldflags("-T$(projectdir)/kernel/arch/$(arch)/$(port)/linker.lds", { force = true })

	on_run(function() end)
