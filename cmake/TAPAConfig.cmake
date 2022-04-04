find_package(Boost 1.59 COMPONENTS coroutine stacktrace_basic)
find_package(FRT REQUIRED)

include("${CMAKE_CURRENT_LIST_DIR}/TAPATargets.cmake")
include("${CMAKE_CURRENT_LIST_DIR}/TAPACCConfig.cmake")

execute_process(
  COMMAND frt_get_xlnx_env XILINX_HLS OUTPUT_VARIABLE XLNX_INCLUDE_PATH
)
string(
  REGEX
  REPLACE "^XILINX_HLS=" "" XLNX_INCLUDE_PATH "${XLNX_INCLUDE_PATH}/include"
)
if(IS_DIRECTORY ${XLNX_INCLUDE_PATH})
  set_target_properties(
    tapa::tapa tapa::tapa_shared
    PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${XLNX_INCLUDE_PATH}"
  )
  message(STATUS "Using XILINX_HLS include path: ${XLNX_INCLUDE_PATH}")
endif()
