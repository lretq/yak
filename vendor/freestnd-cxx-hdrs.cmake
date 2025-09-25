if(YAK_ARCH STREQUAL "x86_64")
    yak_add_includes(
        freestnd-cxx-hdrs/x86_64/include
    )
elseif(YAK_ARCH STREQUAL "riscv64")
    yak_add_includes(
        freestnd-cxx-hdrs/riscv64/include
    )
elseif(YAK_ARCH STREQUAL "aarch64")
    yak_add_includes(
        freestnd-cxx-hdrs/aarch64/include
    )
endif()
