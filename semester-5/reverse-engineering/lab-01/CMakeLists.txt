cmake_minimum_required(VERSION 3.20)

project(crackme_lab1 C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(crackme main.c)

add_executable(crackme_protected protected_main.c)
target_compile_options(crackme_protected PRIVATE -Wall -Wextra -O0 -fno-toplevel-reorder)
target_link_libraries(crackme_protected PRIVATE iphlpapi)

add_executable(crackme_for_upx пункты13-15/protected_for_upx.c)
target_compile_options(crackme_for_upx PRIVATE -Wall -Wextra -O0 -fno-toplevel-reorder)
target_link_libraries(crackme_for_upx PRIVATE iphlpapi)
