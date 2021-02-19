function(add_tapa_target target_name)
  # Generate Xilinx object file from TAPA C++.
  #
  # The added target will have the following properties:
  #
  # * FILE_NAME
  # * TARGET
  # * KERNEL
  # * PLATFORM
  #
  # Positional Arguments:
  #
  # * target_name: Name of the added cmake target.
  #
  # Required Named Arguments:
  #
  # * INPUT: Input filename.
  # * TOP: Name of the top-level task, which will be used the kernel name.
  # * PLATFORM: SDAccel platform name or path.
  #
  # Optional Named Arguments:
  #
  # * OUTPUT: Optional, output filename, default to ${TOP}.${PLATFORM}.hw.xo
  # * DRAM_MAPPING: A list of mappings from variable name to DDR banks (e.g.
  #   gmem0:DDR[0]).
  # * TAPAC: Optional, path to the tapac executable.
  # * TAPACC: Optional, path to the tapacc executable.
  # * CFLAGS: Optional, cflags for kernel, space separated.
  # * FRT_INTERFACE: Optional, output FRT interface filename.
  # * CLOCK_PERIOD: Optional, override the clock period.
  # * PART_NUM: Optional, override the part number.
  # * DIRECTIVE: Optional, apply partitioning directives.
  # * CONNECTIVITY: Optional, specifies DRAM connections using Xilinx's ini.
  # * CONSTRAINT: Optional if DIRECTIVE is not set, generate partitioning
  #   constraints.
  cmake_parse_arguments(
    TAPA
    ""
    "OUTPUT;INPUT;TOP;PLATFORM;TAPAC;TAPACC;CFLAGS;FRT_INTERFACE;CLOCK_PERIOD;PART_NUM;DIRECTIVE;CONNECTIVITY;CONSTRAINT"
    "DRAM_MAPPING"
    ${ARGN})
  if(NOT TAPA_INPUT)
    message(FATAL_ERROR "INPUT not specified")
  endif()
  if(NOT TAPA_PLATFORM)
    message(FATAL_ERROR "PLATFORM not specified")
  endif()
  if(NOT TAPA_TOP)
    message(FATAL_ERROR "TOP not specified")
  endif()

  if(NOT TAPA_OUTPUT)
    set(TAPA_OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/${TAPA_TOP}.${TAPA_PLATFORM}.hw.xo)
  endif()
  get_filename_component(TAPA_INPUT ${TAPA_INPUT} ABSOLUTE)
  get_filename_component(TAPA_OUTPUT ${TAPA_OUTPUT} ABSOLUTE)

  if(TAPA_TAPAC)
    set(TAPAC ${TAPA_TAPAC})
  endif()
  if(NOT TAPAC)
    find_program(TAPAC tapac PATHS $ENV{HOME}/.local/bin)
  endif()
  if(NOT TAPAC)
    message(FATAL_ERROR "cannot find tapac")
  endif()

  if(TAPA_TAPACC)
    set(TAPACC ${TAPA_TAPACC})
  endif()
  if(NOT TAPACC)
    find_program(TAPACC tapacc)
  endif()
  if(NOT TAPACC)
    message(FATAL_ERROR "cannot find tapacc")
  endif()

  if(TAPA_DIRECTIVE)
    if(NOT TAPA_CONSTRAINT)
      message(FATAL_ERROR "CONSTRAINT not set but DIRECTIVE is")
    endif()
  endif()

  set(tapac_cmd ${TAPAC} ${TAPA_INPUT})
  list(APPEND tapac_cmd --top ${TAPA_TOP})
  list(APPEND tapac_cmd --tapacc ${TAPACC})
  list(APPEND tapac_cmd --platform ${TAPA_PLATFORM})
  list(APPEND tapac_cmd --output ${TAPA_OUTPUT})
  list(APPEND tapac_cmd --work-dir ${TAPA_OUTPUT}.tapa)
  if(TAPA_CFLAGS)
    list(APPEND tapac_cmd --cflags=${TAPA_CFLAGS})
  endif()
  if(TAPA_FRT_INTERFACE)
    list(APPEND tapac_cmd --frt-interface ${TAPA_FRT_INTERFACE})
  endif()
  if(TAPA_CLOCK_PERIOD)
    list(APPEND tapac_cmd --clock-period ${TAPA_CLOCK_PERIOD})
  endif()
  if(TAPA_PART_NUM)
    list(APPEND tapac_cmd --part-num ${TAPA_PART_NUM})
  endif()
  if(TAPA_DIRECTIVE)
    list(APPEND tapac_cmd --directive ${TAPA_DIRECTIVE})
  endif()
  if(TAPA_CONNECTIVITY)
    list(APPEND tapac_cmd --connectivity ${TAPA_CONNECTIVITY})
  endif()
  if(TAPA_CONSTRAINT)
    list(APPEND tapac_cmd --constraint ${TAPA_CONSTRAINT})
  endif()

  add_custom_command(
    OUTPUT ${TAPA_OUTPUT} ${TAPA_FRT_INTERFACE}
    COMMAND ${tapac_cmd}
    DEPENDS ${TAPA_INPUT}
    VERBATIM)

  add_custom_target(${target_name} DEPENDS ${TAPA_OUTPUT})
  set_target_properties(
    ${target_name}
    PROPERTIES FILE_NAME ${TAPA_OUTPUT} TARGET hw KERNEL ${TAPA_TOP}
               PLATFORM ${TAPA_PLATFORM} DRAM_MAPPING "${TAPA_DRAM_MAPPING}")
endfunction()
