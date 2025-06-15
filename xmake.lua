-- stylua: ignore start

add_rules("mode.release", "mode.releasedbg", "mode.minsizerel", "mode.debug")

set_languages("gnu17", "c++23")

add_rules("plugin.compile_commands.autoupdate", {outputdir = "build/"})

set_allowedarchs("x86_64", "riscv64")
set_defaultarchs("x86_64")

set_allowedplats("yak")
set_defaultplat("yak")

set_defaultmode("releasedbg")

includes("toolchains/clang.lua")
set_toolchains("yak-clang")

includes("dependencies/xmake.lua")

includes("kernel/xmake.lua")

target("iso")
	set_default(false)
	set_kind("phony")
	add_deps("yak.elf")

	on_build(function(target)
		import("core.project.project")

		local kernel = project.target("yak.elf")

		os.execv("./tools/mkiso.sh", {
			target:arch(),
			kernel:targetdir()
		})
	end)

target("qemu")
	set_default(true)
	set_kind("phony")
	add_deps("iso")
	on_run(function (target)
		import("core.project.project")
		local kernel = project.target("yak.elf")
		os.execv("./tools/qemu.sh", {
			"-sk",
			target:arch(),
			kernel:targetdir()
		})
	end)

target("qemu-debug")
	set_default(false)
	set_kind("phony")
	add_deps("iso")
	on_run(function (target)
		import("core.project.project")
		local kernel = project.target("yak.elf")
		os.execv("./tools/qemu.sh", {
			"-sD",
			target:arch(),
			kernel:targetdir()
		})
	end)

target("qemu-gdb")
	set_default(false)
	set_kind("phony")
	add_deps("iso")
	on_run(function (target)
		import("core.project.project")
		local kernel = project.target("yak.elf")
		os.execv("./tools/qemu.sh", {
			"-sDP",
			target:arch(),
			kernel:targetdir()
		})
	end)
