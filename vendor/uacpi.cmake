include(uacpi/uacpi.cmake)
target_sources(kernel PRIVATE ${UACPI_SOURCES})
target_include_directories(kernel PUBLIC ${UACPI_INCLUDES})
