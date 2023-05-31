cmake_minimum_required(VERSION 3.26)
add_executable (ShortSwordSlicer "ShortSwordSlicer.cpp" "ConcatenatedHeader.h" "sss.o")

project("ShortSwordSlicer")
set(CMAKE_EXE_LINKER_FLAGS "-static -Wl,--stack,536870912 -llua")
set(CMAKE_BUILD_TYPE Release)