cmake_minimum_required(VERSION 3.20)
project(oop_lab_01 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_executable(oop_lab_01 main.cpp MyString.cpp)

option(BUILD_PYTHON_WRAPPER "Build the Python wrapper" OFF)

if(BUILD_PYTHON_WRAPPER)
    find_package(Python COMPONENTS Interpreter Development.Module REQUIRED)
    find_package(pybind11 CONFIG REQUIRED)
    pybind11_add_module(mystring MyString_wrapper.cpp MyString.cpp)
endif()
