cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)

find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
  set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY})
endif()

project(tlp)

add_subdirectory(backend)

add_library(tlp)
target_sources(
  tlp
  PUBLIC src/tlp.h src/tlp/stream.h src/tlp/mmap.h
         src/tlp/synthesizable/traits.h src/tlp/synthesizable/util.h
  PRIVATE src/tlp.cpp)
target_compile_features(tlp PUBLIC cxx_std_11)
target_include_directories(tlp PUBLIC src)
target_link_libraries(
  tlp
  INTERFACE glog
  PUBLIC pthread)
set_target_properties(tlp PROPERTIES OUTPUT_NAME tlp)

include(GNUInstallDirs)
install(
  TARGETS tlp tlpcc
  EXPORT TLPTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(
  DIRECTORY "${CMAKE_SOURCE_DIR}/src/"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING
  PATTERN "*.h")

set(CPACK_PACKAGE_NAME hlstlp)
set(CPACK_PACKAGE_VERSION_MAJOR 0)
set(CPACK_PACKAGE_VERSION_MINOR 0)
set(CPACK_PACKAGE_VERSION_PATCH 20200418.1)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "High-Level Synthesis Task-Level Parallelization")

set(CPACK_DEBIAN_PACKAGE_DEPENDS "iverilog, libclang-dev, libgoogle-glog-dev")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Blaok Chi")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

set(CPACK_GENERATOR TGZ DEB)
include(CPack)

include(cmake/TLPConfig.cmake)
enable_testing()
add_subdirectory(apps/bandwidth)
add_subdirectory(apps/cannon)
add_subdirectory(apps/graph)
add_subdirectory(apps/jacobi)
add_subdirectory(apps/page-rank)
add_subdirectory(apps/vadd)
