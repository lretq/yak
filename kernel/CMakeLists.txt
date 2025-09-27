add_executable(kernel)

add_subdirectory(src)
add_subdirectory(io)
add_subdirectory(fs)

add_subdirectory(arch/generic-limine)
add_subdirectory(arch/x86_64)

yak_add_includes(
    include
    arch/x86_64/include
)

target_compile_options(kernel
    PRIVATE -ffreestanding
    PRIVATE -nostdinc
    PRIVATE -fno-stack-protector
    PRIVATE -fno-omit-frame-pointer
    PRIVATE -fno-strict-aliasing
    PRIVATE -fstrict-vtable-pointers
    PRIVATE -mgeneral-regs-only
    PRIVATE -Wall
    PRIVATE -Wextra
    PRIVATE -Wno-parentheses-equality
    PRIVATE -ffunction-sections
    PRIVATE -fdata-sections
    PRIVATE -pipe
    PRIVATE -fno-threadsafe-statics
    PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-rtti>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fno-exceptions>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fsized-deallocation>
    PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fcheck-new>
)

target_link_options(kernel
    PRIVATE -nostdlib
    PRIVATE -T${CMAKE_CURRENT_LIST_DIR}/arch/x86_64/linker.lds
    PRIVATE -static
    PRIVATE -Wl,-zmax-page-size=0x1000
    PRIVATE -Wl,--gc-sections
)

target_compile_definitions(kernel 
    PUBLIC
	x86_64
	ARCH="x86_64"
	CONFIG_DEBUG=1
	CONFIG_UBSAN=1

	UACPI_NATIVE_ALLOC_ZEROED=1
	UACPI_SIZED_FREES=1
	FLANTERM_FB_DISABLE_BUMP_ALLOC=1
    PRIVATE
	__YAK_PRIVATE__
)

install(TARGETS kernel
    RUNTIME DESTINATION share/yak
)
