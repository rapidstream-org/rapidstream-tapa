cmake_minimum_required(VERSION 3.14)

if(NOT PROJECT_NAME)
  project(tapa-apps-gemv)
endif()

find_package(gflags REQUIRED)

include(${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/apps.cmake)

add_executable(gemv)
target_sources(gemv PRIVATE gemv-host.cpp gemv.cpp)
target_link_libraries(gemv PRIVATE ${TAPA} gflags)
add_test(NAME gemv COMMAND gemv)

if(SDx_FOUND)
  if (${PLATFORM} MATCHES "_(u50|u280)_")
    set(CONNECTIVITY ${CMAKE_CURRENT_SOURCE_DIR}/link_config.hbm.ini)
  else()
    set(CONNECTIVITY ${CMAKE_CURRENT_SOURCE_DIR}/link_config.ddr.ini)
  endif()
  message(STATUS "Using ${CONNECTIVITY} for gemv on ${PLATFORM}")

  add_tapa_target(
    gemv-hw-xo
    ${TAPA_ARGS}
    INPUT gemv.cpp
    TOP Gemv
    CONNECTIVITY ${CONNECTIVITY}
    PLATFORM ${PLATFORM}
  )

  add_xocc_hw_link_targets(
    ${CMAKE_CURRENT_BINARY_DIR}
    ${VPP_ARGS}
    --config=${CONNECTIVITY}
    INPUT gemv-hw-xo
    HW_EMU_XCLBIN hw_emu_xclbin
    HW_XCLBIN hw_xclbin
  )

  add_custom_target(
    gemv-cosim
    COMMAND $<TARGET_FILE:gemv>
            --bitstream=$<TARGET_PROPERTY:${hw_emu_xclbin},FILE_NAME>
    DEPENDS gemv ${hw_emu_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_custom_target(
    gemv-hw
    COMMAND $<TARGET_FILE:gemv>
            --bitstream=$<TARGET_PROPERTY:${hw_xclbin},FILE_NAME>
    DEPENDS gemv ${hw_xclbin}
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})

  add_test(NAME gemv-cosim COMMAND ${CMAKE_COMMAND} --build ${CMAKE_BINARY_DIR}
                                  --target gemv-cosim)
endif()
