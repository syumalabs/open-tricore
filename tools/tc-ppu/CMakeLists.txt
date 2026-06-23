# tc-ppu build file. Copyright 2026 Syuma Labs. Apache-2.0.
# Runs code on the PPU (ARC EV71) over MCD via the shared tcmcd layer. If the
# DAS package under vendor/ is absent, tcmcd is not built and this skips too.

if(NOT TARGET ppu)
    message(STATUS "tc-ppu skipped, ppu helper (DAS package) not available")
    return()
endif()

add_executable(tc-ppu main.c)
target_link_libraries(tc-ppu PRIVATE ppu)
set_target_properties(tc-ppu PROPERTIES
    BUILD_RPATH   "${TC_DAS_LIBDIR}"
    INSTALL_RPATH "${TC_DAS_LIBDIR}")
install(TARGETS tc-ppu RUNTIME DESTINATION bin)
