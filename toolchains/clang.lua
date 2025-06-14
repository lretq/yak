local function multi_insert(list, ...)
	for idx, val in ipairs({ ... }) do
		list[#list + 1] = val
	end
end

option("lto")
set_default(false)
set_showmenu(true)
set_description("Enable ThinLTO")

toolchain("yak-clang")
set_kind("standalone")
set_toolset("cc", "clang")
set_toolset("ld", "ld.lld", "lld")
set_toolset("sh", "clang")
set_toolset("ar", "llvm-ar", "ar")
set_toolset("ex", "llvm-ar", "ar")
set_toolset("strip", "llvm-strip", "strip")
set_toolset("mm", "clang")
set_toolset("mxx", "clang")
set_toolset("as", "clang")

on_check(function(toolchain)
	return import("lib.detect.find_tool")("clang")
end)

on_load(function(toolchain)
	local cx_args = {
		"-ffreestanding",
		"-fno-stack-protector",
		"-fno-omit-frame-pointer",
		"-fno-strict-aliasing",
		"-fstrict-vtable-pointers",
		"-mgeneral-regs-only",
		"-nostdinc",
		"-Wall",
		"-Wextra",
		"-Wno-parentheses-equality",
		"-ffunction-sections",
		"-fdata-sections",
	}
	local c_args = {}
	local ld_args = {
		"-nostdlib",
		"-static",
		"-znoexecstack",
		"-zmax-page-size=0x1000",
		"--gc-sections",
	}
	local sh_args = {
		"-fuse-ld=lld",
		"-Wl,-shared",
	}

	if get_config("lto") then
		multi_insert(cx_args, "-flto=thin", "-funified-lto")
		table.insert(ld_args, "--lto=thin")
		multi_insert(sh_args, "-flto=thin", "-funified-lto", "-Wl,--lto=full")
	end

	local target = ""

	if is_arch("x86_64") then
		target = "x86_64-unknown-none"
		multi_insert(cx_args, "-mno-red-zone", "-mno-mmx", "-mno-sse", "-mno-sse2", "-mno-80387", "-mcmodel=kernel")
	elseif is_arch("riscv64") then
		target = "riscv64-unknown-none"
		multi_insert(cx_args, "-march=rv64imac", "-mabi=lp64", "-mno-relax")
		multi_insert(ld_args, "-melf64lriscv", "--no-relax")
	end

	table.insert(cx_args, "--target=" .. target)
	table.insert(sh_args, "--target=" .. target)

	toolchain:add("cxflags", cx_args, { force = true })
	toolchain:add("cflags", c_args, { force = true })

	toolchain:add("asflags", cx_args, { force = true })
	toolchain:add("asflags", c_args, { force = true })

	toolchain:add("ldflags", ld_args, { force = true })
	toolchain:add("shflags", sh_args, { force = true })
end)
toolchain_end()
