# tc-load build file. Copyright 2026 Syuma Labs. Apache-2.0.
# Uses the shared tcmcd MCD layer, which links the proprietary Infineon MCD
# library from the DAS package under vendor/. If that package is absent, tcmcd
# is not built, so this target skips itself too.

if(NOT TARGET tcmcd)
    message(STATUS "tc-load skipped, tcmcd (DAS package) not available")
    return()
endif()

add_executable(tc-load main.c)
target_link_libraries(tc-load PRIVATE tcmcd)
set_target_properties(tc-load PROPERTIES
    BUILD_RPATH   "${TC_DAS_LIBDIR}"
    INSTALL_RPATH "${TC_DAS_LIBDIR}")
install(TARGETS tc-load RUNTIME DESTINATION bin)
