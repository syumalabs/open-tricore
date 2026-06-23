# tcmcd, shared MCD access layer for the host tools.
# Copyright 2026 Syuma Labs. Apache-2.0.
# Links the proprietary Infineon MCD library from the DAS package under vendor/.
# That package is user-supplied, so this target is skipped when it is absent and
# the tools that depend on it skip themselves in turn.

file(GLOB DAS_DIRS "${CMAKE_SOURCE_DIR}/vendor/das/opt/Tools/DAS/*")
list(SORT DAS_DIRS)
list(LENGTH DAS_DIRS _n)
if(_n EQUAL 0)
    message(STATUS "tcmcd skipped, vendor DAS package not found")
    return()
endif()
list(GET DAS_DIRS -1 DAS_DIR)

if(NOT EXISTS "${DAS_DIR}/include/mcd_api.h" OR NOT EXISTS "${DAS_DIR}/lib/libmcdxdas.so")
    message(STATUS "tcmcd skipped, MCD headers or library missing under ${DAS_DIR}")
    return()
endif()

add_library(tcmcd STATIC tcmcd.c)
target_include_directories(tcmcd PUBLIC "${DAS_DIR}/include" "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(tcmcd PUBLIC "${DAS_DIR}/lib/libmcdxdas.so")
# Export the lib dir so executables can set their rpath to find libmcdxdas.so.
set(TC_DAS_LIBDIR "${DAS_DIR}/lib" CACHE INTERNAL "DAS library directory")

# ppu, host-side PPU helper layered on tcmcd. Used by tc-ppu.
add_library(ppu STATIC ppu.c)
target_link_libraries(ppu PUBLIC tcmcd)
