cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)

find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
  set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY})
endif()

project(tlp)

add_subdirectory(backend)

find_package(Boost 1.59 COMPONENTS coroutine)

add_library(tlp_static STATIC)
add_library(tlp ALIAS tlp_static)
add_library(tlp_shared SHARED)
target_sources(tlp_static PRIVATE src/tlp.cpp)
target_sources(tlp_shared PRIVATE src/tlp.cpp)
target_compile_features(tlp_static PUBLIC cxx_std_11)
target_compile_features(tlp_shared PUBLIC cxx_std_11)
if(Boost_FOUND)
  target_compile_definitions(tlp_static
                             PRIVATE BOOST_VERSION=${Boost_VERSION_MACRO})
  target_compile_definitions(tlp_shared
                             PRIVATE BOOST_VERSION=${Boost_VERSION_MACRO})
endif()
target_include_directories(
  tlp_static PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
                    ${Boost_INCLUDE_DIRS})
target_include_directories(
  tlp_shared PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
                    ${Boost_INCLUDE_DIRS})
target_link_libraries(
  tlp_static
  PRIVATE ${Boost_LIBRARIES}
  INTERFACE glog
  PUBLIC pthread)
target_link_libraries(
  tlp_shared
  PRIVATE ${Boost_LIBRARIES}
  INTERFACE glog
  PUBLIC pthread)
set_target_properties(tlp_static tlp_shared
                      PROPERTIES OUTPUT_NAME tlp POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)
install(
  TARGETS tlp_static tlp_shared
  EXPORT TLPTargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS tlpcc RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(
  DIRECTORY "${CMAKE_SOURCE_DIR}/src/"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING
  PATTERN "*.h")

set_target_properties(tlp_static PROPERTIES EXPORT_NAME tlp)
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
set(CPACK_PACKAGE_VERSION_PATCH 20200711.1)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY
    "High-Level Synthesis Task-Level Parallelization")

string(JOIN ", " CPACK_DEBIAN_PACKAGE_DEPENDS "clang-8" "iverilog"
       "libgoogle-glog-dev")
if(Boost_FOUND)
  string(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS
         ", libboost-coroutine-dev (>= 1.59)")
endif()
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "libfrt-dev")
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
