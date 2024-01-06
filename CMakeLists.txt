cmake_minimum_required(VERSION 3.14)
cmake_policy(SET CMP0076 NEW)
if(POLICY CMP0111)
  cmake_policy(SET CMP0111 NEW)
endif()
if(POLICY CMP0135)
  cmake_policy(SET CMP0135 NEW)
endif()

file(STRINGS VERSION VERSION)
project(tapa VERSION ${VERSION})

option(TAPA_BUILD_BACKEND "Build TAPA backend" ON)
if(TAPA_BUILD_BACKEND)
  message(STATUS "Building TAPA with backend")
  add_subdirectory(backend)
endif()

# If change, also change `cmake/TAPAConfig.cmake`.
find_package(Boost 1.59 COMPONENTS coroutine stacktrace_basic)
find_package(FRT REQUIRED)

add_library(tapa_static STATIC)
add_library(tapa ALIAS tapa_static)
add_library(tapa_shared SHARED)
target_sources(tapa_static PRIVATE src/tapa/host/tapa.cpp)
target_sources(tapa_shared PRIVATE src/tapa/host/tapa.cpp)
target_compile_features(tapa_static PUBLIC cxx_std_17)
target_compile_features(tapa_shared PUBLIC cxx_std_17)
target_include_directories(
  tapa_static PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
target_include_directories(
  tapa_shared PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>)
target_link_libraries(
  tapa_static
  INTERFACE frt::frt
  PUBLIC glog pthread)
target_link_libraries(
  tapa_shared
  INTERFACE frt::frt
  PUBLIC glog pthread)
set_target_properties(tapa_static tapa_shared
                      PROPERTIES OUTPUT_NAME tapa POSITION_INDEPENDENT_CODE ON)

if(Boost_COROUTINE_FOUND)
  message(STATUS "Building TAPA with coroutine")
  target_compile_definitions(tapa_static PRIVATE TAPA_ENABLE_COROUTINE=1)
  target_compile_definitions(tapa_shared PRIVATE TAPA_ENABLE_COROUTINE=1)
  target_link_libraries(
    tapa_static PRIVATE Boost::boost ${Boost_COROUTINE_LIBRARY}
                        ${Boost_CONTEXT_LIBRARY})
  target_link_libraries(
    tapa_shared PRIVATE Boost::boost ${Boost_COROUTINE_LIBRARY}
                        ${Boost_CONTEXT_LIBRARY})
  list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS
       "libboost-coroutine-dev(>=${Boost_VERSION_STRING})")
  list(APPEND CPACK_RPM_PACKAGE_REQUIRES
       "boost${Boost_VERSION_MAJOR}${Boost_VERSION_MINOR}-coroutine")
endif()

if(Boost_STACKTRACE_BASIC_FOUND)
  message(STATUS "Building TAPA with stacktrace")
  target_compile_definitions(tapa_static PRIVATE TAPA_ENABLE_STACKTRACE=1)
  target_compile_definitions(tapa_shared PRIVATE TAPA_ENABLE_STACKTRACE=1)
  target_link_libraries(
    tapa_static PRIVATE Boost::boost ${Boost_STACKTRACE_BASIC_LIBRARY} dl)
  target_link_libraries(
    tapa_shared PRIVATE Boost::boost ${Boost_STACKTRACE_BASIC_LIBRARY} dl)
  list(APPEND CPACK_DEBIAN_PACKAGE_DEPENDS
       "libboost-stacktrace-dev(>=${Boost_VERSION_STRING})")
  list(APPEND CPACK_RPM_PACKAGE_REQUIRES
       "boost${Boost_VERSION_MAJOR}${Boost_VERSION_MINOR}-stacktrace")
endif()

include(GNUInstallDirs)
install(
  TARGETS tapa_static tapa_shared
  EXPORT TAPATargets
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
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
set(ConfigFiles cmake/TAPAConfig.cmake)
if(TAPA_BUILD_BACKEND)
  install(TARGETS tapacc RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
  install(
    PROGRAMS ${CMAKE_BINARY_DIR}/backend/tapa-clang
    DESTINATION ${CMAKE_INSTALL_BINDIR}
  )
  list(APPEND ConfigFiles cmake/TAPACCConfig.cmake)
endif()
install(
  EXPORT TAPATargets
  FILE TAPATargets.cmake
  NAMESPACE tapa::
  DESTINATION ${ConfigPackageLocation})
install(
  FILES ${ConfigFiles}
  DESTINATION ${ConfigPackageLocation}
  COMPONENT Devel)

set(CPACK_PACKAGE_NAME tapa)
set(CPACK_PACKAGE_VERSION ${CMAKE_PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Task-Parallel High-Level Synthesis")

list(
  APPEND
  CPACK_DEBIAN_PACKAGE_DEPENDS
  "g++" # required by tapac for dependence analysis
  "iverilog" # required by tapac for RTL instrumentation
  "libfrt-dev (>= 0.0.20220626.1)" # required by libtapa for host-kernel
                                   # interface
  "libgoogle-glog-dev" # required by libtapa for logging messages
  "rsync" # required by AutoBridge DSE
)
list(SORT CPACK_DEBIAN_PACKAGE_DEPENDS)
list(JOIN CPACK_DEBIAN_PACKAGE_DEPENDS ", " CPACK_DEBIAN_PACKAGE_DEPENDS)

set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Blaok Chi")
set(CPACK_DEBIAN_PACKAGE_ARCHITECTURE "amd64")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)

list(
  APPEND
  CPACK_RPM_PACKAGE_REQUIRES
  "frt-devel >= 0.0.20220626.1" # required by libtapa for host-kernel interface
  "gcc-c++" # required by tapac for dependence analysis
  "glog-devel" # required by libtapa for logging messages
  "iverilog" # required by tapac for RTL instrumentation
  "rsync" # required by AutoBridge DSE
)
list(SORT CPACK_RPM_PACKAGE_REQUIRES)
list(JOIN CPACK_RPM_PACKAGE_REQUIRES ", " CPACK_RPM_PACKAGE_REQUIRES)

set(CPACK_RPM_FILE_NAME RPM-DEFAULT)

set(CPACK_GENERATOR
    TGZ DEB
    CACHE STRING "Enable selected CPack generators")
include(CPack)

option(TAPA_BUILD_DOCS "Build TAPA documentations" OFF)
if(TAPA_BUILD_DOCS)
  add_subdirectory(docs)
endif()

if(TAPA_BUILD_BACKEND)
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
endif()
