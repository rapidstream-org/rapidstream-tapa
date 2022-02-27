if(PROJECT_NAME STREQUAL "tapa")
  set(TAPA tapa)
  set(TAPAC PYTHONPATH=${CMAKE_SOURCE_DIR}/backend/python python3 -m tapa.tapac
            --cflags=-I${CMAKE_SOURCE_DIR}/src)
  set(TAPACC $<TARGET_FILE:tapacc>)

  if(EXISTS "$ENV{XILINX_HLS}/include")
    include_directories(SYSTEM "$ENV{XILINX_HLS}/include")
    message(STATUS "Using XILINX_HLS include path: $ENV{XILINX_HLS}/include")
  endif()
else()
  find_package(TAPA REQUIRED)
  find_package(FRT)
  set(TAPA tapa::tapa)
endif()

set(CMAKE_CXX_FLAGS "${CMAKE_C_FLAGS} -Wno-attributes")
set(TAPA_CFLAGS "${TAPA_CFLAGS} -Wno-attributes")
set(PLATFORM xilinx_u250_xdma_201830_2 CACHE STRING "Target FPGA platform")
