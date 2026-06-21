# led-demo build file. Copyright 2026 Syuma Labs. Apache-2.0.
add_executable(led-demo main.cpp)

target_link_libraries(led-demo PRIVATE tas_client)

if (UNIX)
    target_compile_definitions(led-demo PRIVATE UNIX)
    target_link_libraries(led-demo PRIVATE pthread dl)
endif()

install(TARGETS led-demo RUNTIME DESTINATION bin)
