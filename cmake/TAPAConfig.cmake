find_package(Boost 1.59 COMPONENTS coroutine stacktrace_basic)
find_package(FRT REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/TAPATargets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/TAPACCConfig.cmake")

if(EXISTS "$ENV{XILINX_HLS}/include")
  set_target_properties(
    tapa::tapa tapa::tapa_shared
    PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "$ENV{XILINX_HLS}/include"
  )
  message(STATUS "Using XILINX_HLS include path: $ENV{XILINX_HLS}/include")
endif()
