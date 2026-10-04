# Set root project name
set(ROOT_PROJECT_NAME x86linuxextra)

# Set root language standards
set(ROOT_CXX_EXTENSIONS TRUE)
set(ROOT_CXX_STANDARD "20")
set(ROOT_C_EXTENSIONS TRUE)
set(ROOT_C_STANDARD "11")

# DO NOT EDIT THE BELOW!

# Set minimum CMake version required
cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0126 NEW)
macro(override_default var override)
  get_property(
    CURRENT_TYPE
    CACHE ${var}
    PROPERTY TYPE)
  if(NOT DEFINED CURRENT_TYPE)
    set(${var} ${override})
  endif()
endmacro()
macro(update_cache var type)
  get_property(
    CURRENT_TYPE
    CACHE ${var}
    PROPERTY TYPE)
  if(DEFINED CURRENT_TYPE)
    set(type ${CURRENT_TYPE})
  endif()
  get_property(
    CURRENT_HELPSTRING
    CACHE ${var}
    PROPERTY HELPSTRING)
  set(${var}
      ${${var}}
      CACHE ${type} "${CURRENT_HELPSTRING}" FORCE)
endmacro()

# Override default generator
if(CMAKE_GENERATOR STREQUAL "Ninja")
  configure_file(${CMAKE_SOURCE_DIR}/Makefile ./Makefile COPYONLY)
endif()
override_default(CMAKE_EXPORT_COMPILE_COMMANDS TRUE)

# Override default C/C++ compiler
get_property(
  CXX_COMPILER
  CACHE CMAKE_CXX_COMPILER
  PROPERTY VALUE)
get_property(
  C_COMPILER
  CACHE CMAKE_C_COMPILER
  PROPERTY VALUE)
if(NOT DEFINED CXX_COMPILER)
  if(C_COMPILER STREQUAL "gcc")
    override_default(CMAKE_CXX_COMPILER "g++")
  else()
    override_default(CMAKE_CXX_COMPILER "clang++")
  endif()
endif()
if(NOT DEFINED C_COMPILER)
  if(CXX_COMPILER STREQUAL "g++")
    override_default(CMAKE_C_COMPILER "gcc")
  else()
    override_default(CMAKE_C_COMPILER "clang")
  endif()
endif()

# Override default C/C++ standard and extension to use
override_default(CMAKE_CXX_EXTENSIONS ${ROOT_CXX_EXTENSIONS})
override_default(CMAKE_CXX_STANDARD ${ROOT_CXX_STANDARD})
override_default(CMAKE_C_EXTENSIONS ${ROOT_C_EXTENSIONS})
override_default(CMAKE_C_STANDARD ${ROOT_C_STANDARD})

# Set our C/C++ default compiler options
set(C_FLAGS -frecord-gcc-switches -save-temps=obj -fverbose-asm -Wall -Werror)
set(CXX_FLAGS ${C_FLAGS})
set(CLANG_FLAGS -fcolor-diagnostics -Xclang -gsrc-hash=none)
set(GCC_FLAGS -fdiagnostics-color=always)
if(CMAKE_C_COMPILER MATCHES ".*clang.*")
  set(C_FLAGS ${C_FLAGS} ${CLANG_FLAGS})
elseif(CMAKE_C_COMPILER MATCHES ".*gcc.*")
  set(C_FLAGS ${C_FLAGS} ${GCC_FLAGS})
endif()
if(CMAKE_CXX_COMPILER MATCHES ".*clang.+.+.*")
  set(CXX_FLAGS ${CXX_FLAGS} ${CLANG_FLAGS})
elseif(CMAKE_CXX_COMPILER MATCHES "^(g.+.+.*|.*-g.+.+.*)$")
  set(CXX_FLAGS ${CXX_FLAGS} ${GCC_FLAGS})
endif()

# Set our C/C++ `Debug` compiler options
set(FLAGS_DEBUG_COMMON -Og -g3 -gdwarf-5)
set(C_FLAGS_DEBUG ${FLAGS_DEBUG_COMMON})
set(CXX_FLAGS_DEBUG ${C_FLAGS_DEBUG})

# Set our C/C++ `Release` compiler options
set(C_FLAGS_REL_COMMON -DNDEBUG -march=native -ftree-vectorize)
set(CXX_FLAGS_REL_COMMON ${C_FLAGS_REL_COMMON})
set(GCC_FLAGS_REL_COMMON -fvect-cost-model=very-cheap)
if(CMAKE_C_COMPILER MATCHES ".*gcc.*")
  set(C_FLAGS_REL_COMMON ${C_FLAGS_REL_COMMON} ${GCC_FLAGS_REL_COMMON})
endif()
if(CMAKE_CXX_COMPILER MATCHES "^(g.+.+.*|.*-g.+.+.*)$")
  set(CXX_FLAGS_REL_COMMON ${CXX_FLAGS_REL_COMMON} ${GCC_FLAGS_REL_COMMON})
endif()
set(C_FLAGS_RELEASE -O2)
set(C_FLAGS_MINSIZEREL -Os -fmerge-all-constants -ffunction-sections
                       -fdata-sections)
set(C_FLAGS_RELWITHDEBINFO -O2 ${FLAGS_DEBUG_COMMON})
set(CXX_FLAGS_RELEASE ${C_FLAGS_RELEASE} ${CXX_FLAGS_REL_COMMON})
set(CXX_FLAGS_MINSIZEREL ${C_FLAGS_MINSIZEREL} ${CXX_FLAGS_REL_COMMON})
set(CXX_FLAGS_RELWITHDEBINFO ${C_FLAGS_RELWITHDEBINFO} ${CXX_FLAGS_REL_COMMON})
set(C_FLAGS_RELEASE ${C_FLAGS_RELEASE} ${C_FLAGS_REL_COMMON})
set(C_FLAGS_MINSIZEREL ${C_FLAGS_MINSIZEREL} ${C_FLAGS_REL_COMMON})
set(C_FLAGS_RELWITHDEBINFO ${C_FLAGS_RELWITHDEBINFO} ${C_FLAGS_REL_COMMON})

# Set LTO options
set(CLANG_FLAGS_LTO -flto=full -ffat-lto-objects)
set(GCC_FLAGS_LTO -flto -ffat-lto-objects)
if(CMAKE_C_COMPILER MATCHES ".*clang.*")
  set(C_FLAGS_RELEASE ${C_FLAGS_RELEASE} ${CLANG_FLAGS_LTO})
  set(C_FLAGS_MINSIZEREL ${C_FLAGS_MINSIZEREL} ${CLANG_FLAGS_LTO})
  set(C_FLAGS_RELWITHDEBINFO ${C_FLAGS_RELWITHDEBINFO} ${CLANG_FLAGS_LTO})
elseif(CMAKE_C_COMPILER MATCHES ".*gcc.*")
  set(C_FLAGS_RELEASE ${C_FLAGS_RELEASE} ${GCC_FLAGS_LTO})
  set(C_FLAGS_MINSIZEREL ${C_FLAGS_MINSIZEREL} ${GCC_FLAGS_LTO})
  set(C_FLAGS_RELWITHDEBINFO ${C_FLAGS_RELWITHDEBINFO} ${GCC_FLAGS_LTO})
endif()
if(CMAKE_CXX_COMPILER MATCHES ".*clang.+.+.*")
  set(CXX_FLAGS_RELEASE ${CXX_FLAGS_RELEASE} ${CLANG_FLAGS_LTO})
  set(CXX_FLAGS_MINSIZEREL ${CXX_FLAGS_MINSIZEREL} ${CLANG_FLAGS_LTO})
  set(CXX_FLAGS_RELWITHDEBINFO ${CXX_FLAGS_RELWITHDEBINFO} ${CLANG_FLAGS_LTO})
elseif(CMAKE_CXX_COMPILER MATCHES "^(g.+.+.*|.*-g.+.+.*)$")
  set(CXX_FLAGS_RELEASE ${CXX_FLAGS_RELEASE} ${GCC_FLAGS_LTO})
  set(CXX_FLAGS_MINSIZEREL ${CXX_FLAGS_MINSIZEREL} ${GCC_FLAGS_LTO})
  set(CXX_FLAGS_RELWITHDEBINFO ${CXX_FLAGS_RELWITHDEBINFO} ${GCC_FLAGS_LTO})
endif()

# Set our linker flags
override_default(CMAKE_LINKER "lld")
set(LINKER_FLAGS_COMMON -fuse-ld=${CMAKE_LINKER} -Wl,--as-needed
                        -Wl,--gc-sections -Wl,--no-whole-archive)
set(LINKER_FLAGS_DEBUG -Wl,--export-dynamic)
set(LINKER_FLAGS_RELEASE -Wl,--strip-all)

# Handle build type
override_default(CMAKE_BUILD_TYPE "Debug")
string(REPLACE ";" " " C_FLAGS "${C_FLAGS}")
string(REPLACE ";" " " C_FLAGS_DEBUG "${C_FLAGS_DEBUG}")
string(REPLACE ";" " " C_FLAGS_RELEASE "${C_FLAGS_RELEASE}")
string(REPLACE ";" " " C_FLAGS_MINSIZEREL "${C_FLAGS_MINSIZEREL}")
string(REPLACE ";" " " C_FLAGS_RELWITHDEBINFO "${C_FLAGS_RELWITHDEBINFO}")
string(REPLACE ";" " " CXX_FLAGS "${CXX_FLAGS}")
string(REPLACE ";" " " CXX_FLAGS_DEBUG "${CXX_FLAGS_DEBUG}")
string(REPLACE ";" " " CXX_FLAGS_RELEASE "${CXX_FLAGS_RELEASE}")
string(REPLACE ";" " " CXX_FLAGS_MINSIZEREL "${CXX_FLAGS_MINSIZEREL}")
string(REPLACE ";" " " CXX_FLAGS_RELWITHDEBINFO "${CXX_FLAGS_RELWITHDEBINFO}")
string(REPLACE ";" " " LINKER_FLAGS_COMMON "${LINKER_FLAGS_COMMON}")
string(REPLACE ";" " " LINKER_FLAGS_DEBUG "${LINKER_FLAGS_DEBUG}")
string(REPLACE ";" " " LINKER_FLAGS_RELEASE "${LINKER_FLAGS_RELEASE}")
override_default(CMAKE_CXX_FLAGS ${CXX_FLAGS})
override_default(CMAKE_CXX_FLAGS_DEBUG ${CXX_FLAGS_DEBUG})
override_default(CMAKE_CXX_FLAGS_RELEASE ${CXX_FLAGS_RELEASE})
override_default(CMAKE_CXX_FLAGS_MINSIZEREL ${CXX_FLAGS_MINSIZEREL})
override_default(CMAKE_CXX_FLAGS_RELWITHDEBINFO ${CXX_FLAGS_RELWITHDEBINFO})
override_default(CMAKE_C_FLAGS ${C_FLAGS})
override_default(CMAKE_C_FLAGS_DEBUG ${C_FLAGS_DEBUG})
override_default(CMAKE_C_FLAGS_RELEASE ${C_FLAGS_RELEASE})
override_default(CMAKE_C_FLAGS_MINSIZEREL ${C_FLAGS_MINSIZEREL})
override_default(CMAKE_C_FLAGS_RELWITHDEBINFO ${C_FLAGS_RELWITHDEBINFO})
override_default(CMAKE_EXE_LINKER_FLAGS ${LINKER_FLAGS_COMMON})
override_default(CMAKE_EXE_LINKER_FLAGS_DEBUG ${LINKER_FLAGS_DEBUG})
override_default(CMAKE_EXE_LINKER_FLAGS_RELEASE ${LINKER_FLAGS_RELEASE})
override_default(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL ${LINKER_FLAGS_RELEASE})
override_default(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO ${LINKER_FLAGS_DEBUG})
override_default(CMAKE_SHARED_LINKER_FLAGS ${LINKER_FLAGS_COMMON})
override_default(CMAKE_SHARED_LINKER_FLAGS_DEBUG ${LINKER_FLAGS_DEBUG})
override_default(CMAKE_SHARED_LINKER_FLAGS_RELEASE ${LINKER_FLAGS_RELEASE})
override_default(CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL ${LINKER_FLAGS_RELEASE})
override_default(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO ${LINKER_FLAGS_DEBUG})
override_default(CMAKE_MODULE_LINKER_FLAGS ${LINKER_FLAGS_COMMON})
override_default(CMAKE_MODULE_LINKER_FLAGS_DEBUG ${LINKER_FLAGS_DEBUG})
override_default(CMAKE_MODULE_LINKER_FLAGS_RELEASE ${LINKER_FLAGS_RELEASE})
override_default(CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL ${LINKER_FLAGS_RELEASE})
override_default(CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO ${LINKER_FLAGS_DEBUG})

# Set output path
override_default(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
override_default(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
override_default(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

# Set install path
override_default(CMAKE_INSTALL_PREFIX "/usr/local")

# Declare top-level project
project(${ROOT_PROJECT_NAME})

# Update cache values at once after project initialization
update_cache(CMAKE_ARCHIVE_OUTPUT_DIRECTORY PATH)
update_cache(CMAKE_BUILD_TYPE STRING)
update_cache(CMAKE_CXX_COMPILER FILEPATH)
update_cache(CMAKE_CXX_EXTENSIONS BOOL)
update_cache(CMAKE_CXX_FLAGS STRING)
update_cache(CMAKE_CXX_FLAGS_DEBUG STRING)
update_cache(CMAKE_CXX_FLAGS_MINSIZEREL STRING)
update_cache(CMAKE_CXX_FLAGS_RELEASE STRING)
update_cache(CMAKE_CXX_FLAGS_RELWITHDEBINFO STRING)
update_cache(CMAKE_CXX_STANDARD STRING)
update_cache(CMAKE_C_COMPILER FILEPATH)
update_cache(CMAKE_C_EXTENSIONS BOOL)
update_cache(CMAKE_C_FLAGS STRING)
update_cache(CMAKE_C_FLAGS_DEBUG STRING)
update_cache(CMAKE_C_FLAGS_MINSIZEREL STRING)
update_cache(CMAKE_C_FLAGS_RELEASE STRING)
update_cache(CMAKE_C_FLAGS_RELWITHDEBINFO STRING)
update_cache(CMAKE_C_STANDARD STRING)
update_cache(CMAKE_EXE_LINKER_FLAGS STRING)
update_cache(CMAKE_EXE_LINKER_FLAGS_DEBUG STRING)
update_cache(CMAKE_EXE_LINKER_FLAGS_MINSIZEREL STRING)
update_cache(CMAKE_EXE_LINKER_FLAGS_RELEASE STRING)
update_cache(CMAKE_EXE_LINKER_FLAGS_RELWITHDEBINFO STRING)
update_cache(CMAKE_EXPORT_COMPILE_COMMANDS BOOL)
update_cache(CMAKE_INSTALL_PREFIX PATH)
update_cache(CMAKE_LIBRARY_OUTPUT_DIRECTORY PATH)
update_cache(CMAKE_LINKER FILEPATH)
update_cache(CMAKE_MODULE_LINKER_FLAGS STRING)
update_cache(CMAKE_MODULE_LINKER_FLAGS_DEBUG STRING)
update_cache(CMAKE_MODULE_LINKER_FLAGS_MINSIZEREL STRING)
update_cache(CMAKE_MODULE_LINKER_FLAGS_RELEASE STRING)
update_cache(CMAKE_MODULE_LINKER_FLAGS_RELWITHDEBINFO STRING)
update_cache(CMAKE_RUNTIME_OUTPUT_DIRECTORY PATH)
update_cache(CMAKE_SHARED_LINKER_FLAGS STRING)
update_cache(CMAKE_SHARED_LINKER_FLAGS_DEBUG STRING)
update_cache(CMAKE_SHARED_LINKER_FLAGS_MINSIZEREL STRING)
update_cache(CMAKE_SHARED_LINKER_FLAGS_RELEASE STRING)
update_cache(CMAKE_SHARED_LINKER_FLAGS_RELWITHDEBINFO STRING)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
find_package(Threads REQUIRED)

# Resolve external dependencies again when the installed package is consumed.
file(
  WRITE "${CMAKE_CURRENT_BINARY_DIR}/x86linuxextraDependencies.cmake"
  [=[
find_path(LIBBACKTRACE_INCLUDE_DIR backtrace.h REQUIRED)
find_library(LIBBACKTRACE_STATIC NAMES libbacktrace.a REQUIRED)
if(NOT TARGET x86linuxextra::_libbacktrace_headers)
  add_library(x86linuxextra::_libbacktrace_headers INTERFACE IMPORTED GLOBAL)
  set_target_properties(x86linuxextra::_libbacktrace_headers PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${LIBBACKTRACE_INCLUDE_DIR}")
endif()
if(NOT TARGET x86linuxextra::_libbacktrace_static)
  add_library(x86linuxextra::_libbacktrace_static STATIC IMPORTED GLOBAL)
  set_target_properties(x86linuxextra::_libbacktrace_static PROPERTIES
    IMPORTED_LOCATION "${LIBBACKTRACE_STATIC}")
endif()
]=])
include("${CMAKE_CURRENT_BINARY_DIR}/x86linuxextraDependencies.cmake")

add_library(headers INTERFACE)
add_library(x86linuxextra::headers ALIAS headers)
set_target_properties(headers PROPERTIES EXPORT_NAME headers)
target_include_directories(
  headers INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
                    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_compile_definitions(headers INTERFACE _GNU_SOURCE)
target_link_libraries(headers INTERFACE x86linuxextra::_libbacktrace_headers)

set(GENERATED_DIR "${CMAKE_CURRENT_BINARY_DIR}/generated")
set(VERSION_HEADER "${GENERATED_DIR}/version.h")
add_custom_target(
  version-${ROOT_PROJECT_NAME} ALL
  COMMAND
    "${CMAKE_COMMAND}" "-DSOURCE_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
    "-DVERSION_HEADER=${VERSION_HEADER}" -P
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/generate_version.cmake"
  BYPRODUCTS "${VERSION_HEADER}"
  COMMENT "Generating x86linuxextra version"
  VERBATIM)

# Add CMake source subdirectories
add_subdirectory(src)

set(PACKAGE_INSTALL_DIR "${CMAKE_INSTALL_LIBDIR}/cmake/x86linuxextra")
file(
  WRITE "${CMAKE_CURRENT_BINARY_DIR}/x86linuxextraConfig.cmake.in"
  [=[
@PACKAGE_INIT@
include(CMakeFindDependencyMacro)
find_dependency(Threads)
include("${CMAKE_CURRENT_LIST_DIR}/x86linuxextraDependencies.cmake")
if(NOT TARGET x86linuxextra::libx86linuxextra.a)
  include("${CMAKE_CURRENT_LIST_DIR}/x86linuxextraTargets.cmake")
  foreach(target headers libx86linuxextra.a libx86linuxextra.so)
    set_property(TARGET x86linuxextra::${target} PROPERTY IMPORTED_GLOBAL TRUE)
  endforeach()
endif()
check_required_components(x86linuxextra)
]=])
configure_package_config_file(
  "${CMAKE_CURRENT_BINARY_DIR}/x86linuxextraConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/x86linuxextraConfig.cmake"
  INSTALL_DESTINATION "${PACKAGE_INSTALL_DIR}")
export(
  EXPORT x86linuxextraTargets
  FILE "${CMAKE_CURRENT_BINARY_DIR}/x86linuxextraTargets.cmake"
  NAMESPACE x86linuxextra::)
install(
  EXPORT x86linuxextraTargets
  FILE x86linuxextraTargets.cmake
  NAMESPACE x86linuxextra::
  DESTINATION "${PACKAGE_INSTALL_DIR}")
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/x86linuxextraConfig.cmake"
              "${CMAKE_CURRENT_BINARY_DIR}/x86linuxextraDependencies.cmake"
        DESTINATION "${PACKAGE_INSTALL_DIR}")

# Install public header
install(
  DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/
  DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}"
  FILES_MATCHING
  PATTERN "*.h"
  PATTERN "*.hh"
  PATTERN "*.hpp"
  PATTERN "*.hxx")

# Add `uninstall` target
add_custom_target(
  uninstall
  COMMAND ${CMAKE_SOURCE_DIR}/uninstall.sh
  DEPENDS install_manifest.txt)

# Run ldconfig
install(
  CODE "if(NOT DEFINED ENV{DESTDIR} AND CMAKE_INSTALL_PREFIX STREQUAL \"/usr/local\")
  execute_process(COMMAND ldconfig)
endif()")
