# CMakeList.txt : CMake project for v8-etw, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

# Comment out the below 3 set statements to use MSVC on Windows
# Put clang on the path, e.g: \src\github\v8\third_party\llvm-build\Release+Asserts\bin
set(CMAKE_CXX_COMPILER clang-cl.exe)
set(CMAKE_C_COMPILER clang-cl.exe)
set(CMAKE_LINKER lld-link.exe)

set(CMAKE_CXX_FLAGS "/std:c++14")

project(cpp-etw)

add_executable (cpp-etw "main.cc" "example-etw-provider.cc" "example-etw-provider.h" "etw-events.h" "etw-metadata.h")
