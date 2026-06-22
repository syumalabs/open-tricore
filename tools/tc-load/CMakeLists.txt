# tc-load build file. Copyright 2026 Syuma Labs. Apache-2.0.
# Links the proprietary Infineon MCD library from the DAS package under vendor/.
# That package is user-supplied (see scripts/install-tas.sh), so this target is
# skipped cleanly when it is absent.

file(GLOB DAS_DIRS "${CMAKE_SOURCE_DIR}/vendor/das/opt/Tools/DAS/*")
list(SORT DAS_DIRS)
list(LENGTH DAS_DIRS _n)
if(_n EQUAL 0)
    message(STATUS "tc-load skipped, vendor DAS package not found")
    return()
endif()
list(GET DAS_DIRS -1 DAS_DIR)

if(NOT EXISTS "${DAS_DIR}/include/mcd_api.h" OR NOT EXISTS "${DAS_DIR}/lib/libmcdxdas.so")
    message(STATUS "tc-load skipped, MCD headers or library missing under ${DAS_DIR}")
    return()
endif()

add_executable(tc-load main.c)
target_include_directories(tc-load PRIVATE "${DAS_DIR}/include")
target_link_libraries(tc-load PRIVATE "${DAS_DIR}/lib/libmcdxdas.so")
set_target_properties(tc-load PROPERTIES
    BUILD_RPATH   "${DAS_DIR}/lib"
    INSTALL_RPATH "${DAS_DIR}/lib")
install(TARGETS tc-load RUNTIME DESTINATION bin)
