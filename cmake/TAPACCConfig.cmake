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
  # * TAPA_CLI: Optional, path to the tapa executable.
  # * TAPACC: Optional, path to the tapacc executable.
  # * TAPA_CLANG: Optional, path to the tapa-clang executable.
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
    "OUTPUT;INPUT;TOP;PLATFORM;TAPA_CLI;TAPACC;TAPA_CLANG;CFLAGS;FRT_INTERFACE;CLOCK_PERIOD;PART_NUM;DIRECTIVE;CONNECTIVITY;CONSTRAINT"
    ""
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

  if(TAPA_TAPA_CLI)
    set(TAPA_CLI ${TAPA_TAPA_CLI})
  endif()
  if(NOT TAPA_CLI)
    find_program(TAPA_CLI tapa PATHS $ENV{HOME}/.local/bin)
  endif()
  if(NOT TAPA_CLI)
    message(FATAL_ERROR "cannot find tapa")
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

  if(TAPA_TAPA_CLANG)
    set(TAPA_CLANG ${TAPA_TAPA_CLANG})
  endif()
  if(NOT TAPA_CLANG)
    find_program(TAPA_CLANG tapa-clang)
  endif()
  if(NOT TAPA_CLANG)
    message(FATAL_ERROR "cannot find tapa-clang")
  endif()

  set(tapa_cmd ${TAPA_CLI} --work-dir ${TAPA_OUTPUT}.tapa)
  list(APPEND tapa_cmd analyze)
  list(APPEND tapa_cmd -f ${TAPA_INPUT})
  list(APPEND tapa_cmd --top ${TAPA_TOP})
  list(APPEND tapa_cmd --tapacc ${TAPACC})
  list(APPEND tapa_cmd --tapa-clang ${TAPA_CLANG})
  if(TAPA_CFLAGS)
    list(APPEND tapa_cmd --cflags ${TAPA_CFLAGS})
  endif()

  list(APPEND tapa_cmd synth)
  list(APPEND tapa_cmd --platform ${TAPA_PLATFORM})
  if(TAPA_CLOCK_PERIOD)
    list(APPEND tapa_cmd --clock-period ${TAPA_CLOCK_PERIOD})
  endif()
  if(TAPA_PART_NUM)
    list(APPEND tapa_cmd --part-num ${TAPA_PART_NUM})
  endif()

  if(TAPA_CONNECTIVITY AND TAPA_CONSTRAINT)
    list(APPEND tapa_cmd optimize-floorplan)
    list(APPEND tapa_cmd --connectivity ${TAPA_CONNECTIVITY})
  endif()

  # If we have to put unparsed arguments somewhere, this is the best place; most
  # additional arguments are for `tapa synth` and `tapa optimize-floorplan`.
  list(APPEND tapa_cmd ${TAPA_UNPARSED_ARGUMENTS})

  list(APPEND tapa_cmd link)
  if(TAPA_CONNECTIVITY AND TAPA_CONSTRAINT)
    list(APPEND tapa_cmd --floorplan-output ${TAPA_CONSTRAINT})
  endif()

  list(APPEND tapa_cmd pack)
  list(APPEND tapa_cmd --output ${TAPA_OUTPUT})

  add_custom_command(
    OUTPUT ${TAPA_OUTPUT}
    COMMAND ${tapa_cmd}
    DEPENDS ${TAPA_INPUT}
    VERBATIM)

  add_custom_target(${target_name} DEPENDS ${TAPA_OUTPUT})
  set_target_properties(
    ${target_name}
    PROPERTIES FILE_NAME ${TAPA_OUTPUT} TARGET hw KERNEL ${TAPA_TOP}
               PLATFORM ${TAPA_PLATFORM} DRAM_MAPPING "")
endfunction()
