function(add_tlp_target target_name)
  # Generate HLS C++ from TLP C++.
  #
  # The added target will have the following properties:
  #
  # * FILE_NAME
  #
  # Positional Arguments: * target_name: Name of the added cmake target.
  #
  # Named Arguments:
  #
  # * OUTPUT: Output filename.
  # * INPUT: Input filename.
  cmake_parse_arguments(TLP
                        ""
                        "OUTPUT;INPUT;TLPC"
                        ""
                        ${ARGN})
  get_filename_component(TLP_INPUT ${TLP_INPUT} ABSOLUTE)
  get_filename_component(TLP_OUTPUT ${TLP_OUTPUT} ABSOLUTE)

  find_program(CLANG_FORMAT
               NAMES clang-format
                     clang-format-10
                     clang-format-9
                     clang-format-8
                     clang-format-7
                     clang-format-6
                     clang-format-5)

  if(TLP_TLPC)
    set(TLPC ${TLP_TLPC})
  endif()
  if(NOT TLPC)
    find_program(TLPC tlpc)
  endif()
  if(NOT TLPC)
    message(FATAL_ERROR "cannot find tlpc")
  endif()

  set(tlpc_cmd
      ${TLPC}
      ${TLP_INPUT}
      --
      -I${CMAKE_SOURCE_DIR}/src
      -I/usr/lib/clang/10/include
      -I/usr/lib/clang/9/include
      -I/usr/lib/clang/8/include
      -I/usr/lib/clang/7/include
      -I/usr/lib/clang/6/include
      -I/usr/lib/clang/5/include)
  if(CLANG_FORMAT)
    list(APPEND tlpc_cmd "|;${CLANG_FORMAT};-style=Google")
  endif()
  list(APPEND tlpc_cmd "|;sponge;${TLP_OUTPUT}")

  add_custom_command(OUTPUT ${TLP_OUTPUT}
                     COMMAND ${tlpc_cmd}
                     DEPENDS tlpc ${TLP_INPUT}
                     VERBATIM)

  add_custom_target(${target_name} DEPENDS ${TLP_OUTPUT})
  set_target_properties(${target_name} PROPERTIES FILE_NAME ${TLP_OUTPUT})
endfunction()
