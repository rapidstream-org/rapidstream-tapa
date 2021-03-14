cmake_minimum_required(VERSION 3.13)
cmake_policy(SET CMP0076 NEW)

if(${CMAKE_VERSION} VERSION_GREATER_EQUAL "3.15.0")
  cmake_policy(SET CMP0093 NEW)
endif()

find_program(CLANG_TIDY clang-tidy)
if(CLANG_TIDY)
  set(CMAKE_CXX_CLANG_TIDY ${CLANG_TIDY})
endif()

project(tapa)

add_subdirectory(backend)

find_package(Boost 1.59 COMPONENTS coroutine stacktrace_backtrace)
find_package(FRT REQUIRED)

add_library(tapa_static STATIC)
add_library(tapa ALIAS tapa_static)
add_library(tapa_shared SHARED)
target_sources(tapa_static PRIVATE src/tapa.cpp)
target_sources(tapa_shared PRIVATE src/tapa.cpp)
target_compile_features(tapa_static PUBLIC cxx_std_11)
target_compile_features(tapa_shared PUBLIC cxx_std_11)
if(Boost_FOUND)
  if(${CMAKE_VERSION} VERSION_LESS "3.15.0")
    target_compile_definitions(tapa_static
                               PRIVATE BOOST_VERSION=${Boost_VERSION})
    target_compile_definitions(tapa_shared
                               PRIVATE BOOST_VERSION=${Boost_VERSION})
  else()
    target_compile_definitions(tapa_static
                               PRIVATE BOOST_VERSION=${Boost_VERSION_MACRO})
    target_compile_definitions(tapa_shared
                               PRIVATE BOOST_VERSION=${Boost_VERSION_MACRO})
  endif()
endif()

if(Boost_stacktrace_backtrace_FOUND)
  find_library(BACKTRACE_LIBRARY backtrace REQUIRED
               PATHS /usr/lib/gcc/x86_64-linux-gnu/*)
  get_filename_component(BACKTRACE_INCLUDEDIR ${BACKTRACE_LIBRARY} DIRECTORY)
  target_include_directories(tapa_static
                             PRIVATE ${BACKTRACE_INCLUDEDIR}/include)
  target_include_directories(tapa_shared
                             PRIVATE ${BACKTRACE_INCLUDEDIR}/include)
  target_link_libraries(tapa_static PUBLIC ${BACKTRACE_LIBRARY} dl)
  target_link_libraries(tapa_shared PUBLIC ${BACKTRACE_LIBRARY} dl)
else()
  target_compile_definitions(tapa_static PRIVATE BOOST_STACKTRACE_USE_NOOP)
  target_compile_definitions(tapa_shared PRIVATE BOOST_STACKTRACE_USE_NOOP)
endif()

target_include_directories(
  tapa_static PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
                     ${Boost_INCLUDE_DIRS})
target_include_directories(
  tapa_shared PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
                     ${Boost_INCLUDE_DIRS})
target_link_libraries(
  tapa_static
  PRIVATE ${Boost_LIBRARIES}
  INTERFACE glog frt::frt
  PUBLIC pthread)
target_link_libraries(
  tapa_shared
  PRIVATE ${Boost_LIBRARIES}
  INTERFACE glog frt::frt
  PUBLIC pthread)
set_target_properties(tapa_static tapa_shared
                      PROPERTIES OUTPUT_NAME tapa POSITION_INDEPENDENT_CODE ON)

include(GNUInstallDirs)
install(
  TARGETS tapa_static tapa_shared
  EXPORT TAPATargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS tapacc RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(
  DIRECTORY "${CMAKE_SOURCE_DIR}/src/"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
  FILES_MATCHING
  PATTERN "*.h")

set_target_properties(tapa_static PROPERTIES EXPORT_NAME tapa)
export(
  EXPORT TAPATargets
  NAMESPACE tapa::
  FILE ${CMAKE_CURRENT_BINARY_DIR}/tapa/TAPATargets.cmake)
set(ConfigPackageLocation lib/cmake/tapa)
install(
  EXPORT TAPATargets
  FILE TAPATargets.cmake
  NAMESPACE tapa::
  DESTINATION ${ConfigPackageLocation})
install(
  FILES cmake/TAPAConfig.cmake cmake/TAPACCConfig.cmake
  DESTINATION ${ConfigPackageLocation}
  COMPONENT Devel)

set(CPACK_PACKAGE_NAME tapa)
set(CPACK_PACKAGE_VERSION_MAJOR 0)
set(CPACK_PACKAGE_VERSION_MINOR 0)
set(CPACK_PACKAGE_VERSION_PATCH 20210313.2)
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Task-Parallel High-Level Synthesis")

string(
  JOIN
  ", "
  CPACK_DEBIAN_PACKAGE_DEPENDS
  "clang-8"
  "g++"
  "iverilog"
  "libgoogle-glog-dev")
if(Boost_FOUND)
  string(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS
         ", libboost-coroutine-dev (>= 1.59), libboost-stacktrace-dev")
endif()
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "libfrt-dev")
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Blaok Chi")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

set(CPACK_GENERATOR TGZ DEB)
include(CPack)

include(cmake/TAPACCConfig.cmake)
enable_testing()
add_subdirectory(apps/bandwidth)
add_subdirectory(apps/cannon)
add_subdirectory(apps/graph)
add_subdirectory(apps/jacobi)
add_subdirectory(apps/nested-vadd)
add_subdirectory(apps/network)
add_subdirectory(apps/shared-vadd)
add_subdirectory(apps/vadd)
