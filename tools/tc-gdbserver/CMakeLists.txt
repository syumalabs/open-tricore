# tc-gdbserver build file. Copyright 2026 Syuma Labs. Apache-2.0.
# A GDB remote stub over the shared tcmcd MCD layer. Skips itself when the DAS
# package (and therefore tcmcd) is not available.

if(NOT TARGET tcmcd)
    message(STATUS "tc-gdbserver skipped, tcmcd (DAS package) not available")
    return()
endif()

add_executable(tc-gdbserver main.c rsp.c)
target_link_libraries(tc-gdbserver PRIVATE tcmcd)
set_target_properties(tc-gdbserver PROPERTIES
    BUILD_RPATH   "${TC_DAS_LIBDIR}"
    INSTALL_RPATH "${TC_DAS_LIBDIR}")
install(TARGETS tc-gdbserver RUNTIME DESTINATION bin)
