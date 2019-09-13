cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)

find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
  set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY})
endif()

project(tlp)

add_library(tlp)
target_sources(tlp
               PUBLIC
               src/tlp.h
               PRIVATE
               src/tlp.cpp)
target_compile_features(tlp PUBLIC cxx_std_11)
target_include_directories(tlp PUBLIC src)
target_link_libraries(tlp INTERFACE glog PUBLIC pthread)

add_subdirectory(backend)

include(cmake/TLPConfig.cmake)
enable_testing()
add_subdirectory(apps/cannon)
add_subdirectory(apps/graph)
add_subdirectory(apps/jacobi)
add_subdirectory(apps/vadd)
