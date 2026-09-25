if(NOT DEFINED SOURCE_DIR OR NOT DEFINED VERSION_HEADER)
  message(FATAL_ERROR "missing x86linuxextra version arguments")
endif()

execute_process(
  COMMAND sh "${SOURCE_DIR}/generate-version.sh"
  RESULT_VARIABLE VERSION_RES
  OUTPUT_VARIABLE VERSION
  ERROR_QUIET OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT VERSION_RES STREQUAL "0" OR VERSION STREQUAL "")
  set(VERSION unknown)
endif()

get_filename_component(VERSION_DIR "${VERSION_HEADER}" DIRECTORY)
file(MAKE_DIRECTORY "${VERSION_DIR}")
file(CONFIGURE OUTPUT "${VERSION_HEADER}" CONTENT
     "#pragma once\n\nstatic const char version[] = \"@VERSION@\";\n" @ONLY)
