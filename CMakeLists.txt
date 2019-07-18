cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)

find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
  set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY})
endif()

project(tlp)

add_library(tlp INTERFACE)
target_sources(tlp INTERFACE src/tlp.h)
target_compile_features(tlp INTERFACE cxx_std_11)
target_include_directories(tlp INTERFACE src)
target_link_libraries(tlp INTERFACE pthread)

enable_testing()
add_subdirectory(apps/cannon)
add_subdirectory(apps/jacobi)
add_subdirectory(apps/vadd)
