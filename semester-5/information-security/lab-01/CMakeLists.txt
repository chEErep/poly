cmake_minimum_required(VERSION 3.20)

project(bsit_lab1 C)

if(NOT WIN32)
    message(FATAL_ERROR "This laboratory project uses WinSock and Windows IOCP.")
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

add_library(lab_common STATIC
        src/protocol.c
        src/system_info.c
        src/crypto.c
)

target_include_directories(lab_common PUBLIC src)
target_link_libraries(lab_common PUBLIC advapi32)
target_compile_definitions(lab_common PUBLIC
        _WIN32_WINNT=0x0601
        WIN32_LEAN_AND_MEAN
        UNICODE
        _UNICODE
)

add_executable(lab_server src/server.c)
target_link_libraries(lab_server PRIVATE lab_common ws2_32)

add_executable(lab_client src/client.c)
target_link_libraries(lab_client PRIVATE lab_common ws2_32)

add_executable(protocol_tests tests/protocol_tests.c)
target_link_libraries(protocol_tests PRIVATE lab_common)

add_executable(crypto_tests tests/crypto_tests.c)
target_link_libraries(crypto_tests PRIVATE lab_common)

if(MSVC)
    target_compile_options(lab_common PRIVATE /W4)
    target_compile_options(lab_server PRIVATE /W4)
    target_compile_options(lab_client PRIVATE /W4)
    target_compile_options(protocol_tests PRIVATE /W4)
    target_compile_options(crypto_tests PRIVATE /W4)
else()
    target_compile_options(lab_common PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(lab_server PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(lab_client PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(protocol_tests PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(crypto_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()
add_test(NAME protocol_tests COMMAND protocol_tests)
add_test(NAME crypto_tests COMMAND crypto_tests)