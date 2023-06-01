cmake_minimum_required(VERSION 3.26)
add_executable (ShortSwordSlicer "ShortSwordSlicer.cpp" "ConcatenatedHeader.h" "sss.o")

set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR} CACHE STRING "" )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR} CACHE STRING "" )

project("ShortSwordSlicer")
add_subdirectory(external/lua)
target_link_libraries(ShortSwordSlicer PRIVATE lua::lib)
set(CMAKE_EXE_LINKER_FLAGS "-static -Wl,--stack,536870912")
set(CMAKE_BUILD_TYPE Release)