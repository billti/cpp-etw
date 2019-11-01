# CMakeList.txt : CMake project for v8-etw, include source and define
# project specific logic here.
#
cmake_minimum_required (VERSION 3.8)

project(v8-etw)

# set(CMAKE_CXX_COMPILER clang++)
set(CMAKE_CXX_FLAGS "/FAs")

add_executable (v8-etw "main.cc" "v8-etw.cc" "v8-etw.h" "etw-events.h" "etw-metadata.h")
