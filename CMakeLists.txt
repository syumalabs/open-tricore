# open-tricore top-level build.
# Copyright 2026 Syuma Labs. Licensed under the Apache License, Version 2.0.
#
# The upstream TAS client library is built on its own first (see scripts/build.sh),
# because it consumes Conan and assumes it is the top-level CMake project. We then
# link its prebuilt static libraries here via imported targets, keeping the
# third_party submodule pristine.

cmake_minimum_required(VERSION 3.25)
project(open_tricore LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(TAS_CLIENT_API_DIR "${CMAKE_SOURCE_DIR}/third_party/tas_client_api"
    CACHE PATH "Path to the tas_client_api checkout")

set(TAS_INC "${TAS_CLIENT_API_DIR}/src/tas_client")
set(TAS_SOCKET_INC "${TAS_CLIENT_API_DIR}/src/tas_socket")
set(TAS_CLIENT_LIB "${TAS_CLIENT_API_DIR}/build/Release/src/tas_client/libtas_client.a")
set(TAS_SOCKET_LIB "${TAS_CLIENT_API_DIR}/build/Release/src/tas_socket/libtas_socket.a")

if(NOT EXISTS "${TAS_CLIENT_LIB}" OR NOT EXISTS "${TAS_SOCKET_LIB}")
    message(FATAL_ERROR
        "Prebuilt TAS client libraries not found. Run scripts/build.sh first, "
        "it builds third_party/tas_client_api before our tools.")
endif()

add_library(tas_socket STATIC IMPORTED GLOBAL)
set_target_properties(tas_socket PROPERTIES IMPORTED_LOCATION "${TAS_SOCKET_LIB}")

add_library(tas_client STATIC IMPORTED GLOBAL)
set_target_properties(tas_client PROPERTIES
    IMPORTED_LOCATION "${TAS_CLIENT_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${TAS_INC};${TAS_SOCKET_INC}")
target_link_libraries(tas_client INTERFACE tas_socket)

add_subdirectory(tools/common)
add_subdirectory(tools/led-demo)
add_subdirectory(tools/tc-load)
add_subdirectory(tools/tc-gdbserver)
