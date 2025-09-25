include(CMakePrintHelpers)
cmake_print_variables(PROJECT_SOURCE_DIR)
cmake_print_variables(CMAKE_SYSTEM_PROCESSOR)
cmake_print_variables(YAK_ARCH)

if(YAK_ARCH STREQUAL "x86_64")
    yak_add_includes(
        freestnd-c-hdrs/x86_64/include
    )
elseif(YAK_ARCH STREQUAL "riscv64")
    yak_add_includes(
        freestnd-c-hdrs/riscv64/include
    )
elseif(YAK_ARCH STREQUAL "aarch64")
    yak_add_includes(
        freestnd-c-hdrs/aarch64/include
    )
endif()
