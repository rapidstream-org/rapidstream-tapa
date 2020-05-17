cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)

find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
  set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY})
endif()

project(tlp)

add_subdirectory(backend)

find_package(Boost 1.59 REQUIRED COMPONENTS coroutine)

add_library(tlp)
target_sources(
  tlp
  PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src/tlp.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src/tlp/coroutine.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src/tlp/stream.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src/tlp/mmap.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src/tlp/synthesizable/traits.h>
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/src/tlp/synthesizable/util.h>
  PRIVATE src/tlp.cpp)
target_compile_features(tlp PUBLIC cxx_std_11)
target_include_directories(
  tlp PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
             ${Boost_INCLUDE_DIRS})
target_link_libraries(
  tlp
  INTERFACE glog
  PUBLIC pthread ${Boost_LIBRARIES})
set_target_properties(tlp PROPERTIES OUTPUT_NAME tlp POSITION_INDEPENDENT_CODE
                                                     ON)

include(GNUInstallDirs)
install(
  TARGETS tlp
  EXPORT TLPTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS tlpcc RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(
  DIRECTORY "${CMAKE_SOURCE_DIR}/src/"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING
  PATTERN "*.h")

export(
  EXPORT TLPTargets
  NAMESPACE tlp::
  FILE ${CMAKE_CURRENT_BINARY_DIR}/tlp/TLPTargets.cmake)
set(ConfigPackageLocation lib/cmake/tlp)
install(
  EXPORT TLPTargets
  FILE TLPTargets.cmake
  NAMESPACE tlp::
  DESTINATION ${ConfigPackageLocation})
install(
  FILES cmake/TLPConfig.cmake cmake/TLPCCConfig.cmake
  DESTINATION ${ConfigPackageLocation}
  COMPONENT Devel)

set(CPACK_PACKAGE_NAME hlstlp)
set(CPACK_PACKAGE_VERSION_MAJOR 0)
set(CPACK_PACKAGE_VERSION_MINOR 0)
set(CPACK_PACKAGE_VERSION_PATCH 20200513.1)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "High-Level Synthesis Task-Level Parallelization")

string(
  JOIN
  ", "
  CPACK_DEBIAN_PACKAGE_DEPENDS
  "clang"
  "iverilog"
  "libboost-coroutine-dev (>= 1.59)"
  "libgoogle-glog-dev")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Blaok Chi")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

set(CPACK_GENERATOR TGZ DEB)
include(CPack)

include(cmake/TLPCCConfig.cmake)
enable_testing()
add_subdirectory(apps/bandwidth)
add_subdirectory(apps/cannon)
add_subdirectory(apps/graph)
add_subdirectory(apps/jacobi)
add_subdirectory(apps/vadd)
