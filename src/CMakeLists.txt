# libx86linuxextra
project(libx86linuxextra)

# Enlist source from current directory recursively
file(GLOB_RECURSE sources *.c *.cc *.cpp *.cxx)

# Add target library to build with source
add_library(${PROJECT_NAME} SHARED ${sources})
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  add_library(${PROJECT_NAME}.lto SHARED ${sources})
else()
  add_library(${PROJECT_NAME}.dbg SHARED ${sources})
endif()

# Enforce output name format
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "")
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  set_target_properties(${PROJECT_NAME}.lto PROPERTIES PREFIX "")
else()
  set_target_properties(${PROJECT_NAME}.dbg PROPERTIES PREFIX "")
endif()

# Include directory
target_include_directories(${PROJECT_NAME} PUBLIC ${CMAKE_SOURCE_DIR}/include)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  target_include_directories(${PROJECT_NAME}.lto
                             PUBLIC ${CMAKE_SOURCE_DIR}/include)
else()
  target_include_directories(${PROJECT_NAME}.dbg
                             PUBLIC ${CMAKE_SOURCE_DIR}/include)
endif()

# Add build options
target_compile_options(${PROJECT_NAME}
                       PUBLIC $<$<COMPILE_LANGUAGE:C>:${C_FLAGS_NORM}>)
target_compile_options(${PROJECT_NAME}
                       PUBLIC $<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAGS_NORM}>)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  target_compile_options(${PROJECT_NAME}.lto
                         PUBLIC $<$<COMPILE_LANGUAGE:C>:${C_FLAGS_LTO}>)
  target_compile_options(${PROJECT_NAME}.lto
                         PUBLIC $<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAGS_LTO}>)

  # Set link option to strip out all debug & symbol information.
  target_link_options(${PROJECT_NAME} PUBLIC
                      $<$<COMPILE_LANGUAGE:C>:${C_FLAGS_NORM} -Wl,-s>)
  target_link_options(${PROJECT_NAME} PUBLIC
                      $<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAGS_NORM} -Wl,-s>)
  target_link_options(${PROJECT_NAME}.lto PUBLIC
                      $<$<COMPILE_LANGUAGE:C>:${C_FLAGS_LTO} -Wl,-s>)
  target_link_options(${PROJECT_NAME}.lto PUBLIC
                      $<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAGS_LTO} -Wl,-s>)
else()

  target_compile_options(${PROJECT_NAME}.dbg
                         PUBLIC $<$<COMPILE_LANGUAGE:C>:${C_FLAGS_DBG}>)
  target_compile_options(${PROJECT_NAME}.dbg
                         PUBLIC $<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAGS_DBG}>)

  # Set link option to maintain all debug & symbol information.
  target_link_options(${PROJECT_NAME} PUBLIC
                      $<$<COMPILE_LANGUAGE:C>:${C_FLAGS_NORM}>)
  target_link_options(${PROJECT_NAME} PUBLIC
                      $<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAGS_NORM}>)
  target_link_options(${PROJECT_NAME}.dbg PUBLIC
                      $<$<COMPILE_LANGUAGE:C>:${C_FLAGS_DBG}>)
  target_link_options(${PROJECT_NAME}.dbg PUBLIC
                      $<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAGS_DBG}>)
endif()

# Set target-specific build options
set(C_FLAGS_TARGET -mbmi -D_GNU_SOURCE)
if(NO_UMWAIT STREQUAL "yes" AND FORCE_UMWAIT STREQUAL "yes")
  message(
    FATAL_ERROR "Both `NO_UMWAIT` and `FORCE_UMWAIT` defined! Aborting...")
endif()
if(NO_UMWAIT STREQUAL "yes")
  set(C_FLAGS_TARGET ${C_FLAGS_TARGET} -D_NO_UMWAIT)
else()
  set(C_FLAGS_TARGET ${C_FLAGS_TARGET} -mwaitpkg)
endif()
if(FORCE_UMWAIT STREQUAL "yes")
  set(C_FLAGS_TARGET ${C_FLAGS_TARGET} -D_FORCE_UMWAIT)
endif()
set(CXX_FLAGS_TARGET ${C_FLAGS_TARGET})

# Add target-specific build options
target_compile_options(${PROJECT_NAME}
                       PUBLIC $<$<COMPILE_LANGUAGE:C>:${C_FLAGS_TARGET}>)
target_compile_options(${PROJECT_NAME}
                       PUBLIC $<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAGS_TARGET}>)
target_link_options(${PROJECT_NAME} PUBLIC
                    $<$<COMPILE_LANGUAGE:C>:${C_FLAGS_TARGET}>)
target_link_options(${PROJECT_NAME} PUBLIC
                    $<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAGS_TARGET}>)
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  target_compile_options(${PROJECT_NAME}.lto
                         PUBLIC $<$<COMPILE_LANGUAGE:C>:${C_FLAGS_TARGET}>)
  target_compile_options(${PROJECT_NAME}.lto
                         PUBLIC $<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAGS_TARGET}>)
  target_link_options(${PROJECT_NAME}.lto PUBLIC
                      $<$<COMPILE_LANGUAGE:C>:${C_FLAGS_TARGET}>)
  target_link_options(${PROJECT_NAME}.lto PUBLIC
                      $<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAGS_TARGET}>)
else()
  target_compile_options(${PROJECT_NAME}.dbg
                         PUBLIC $<$<COMPILE_LANGUAGE:C>:${C_FLAGS_TARGET}>)
  target_compile_options(${PROJECT_NAME}.dbg
                         PUBLIC $<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAGS_TARGET}>)
  target_link_options(${PROJECT_NAME}.dbg PUBLIC
                      $<$<COMPILE_LANGUAGE:C>:${C_FLAGS_TARGET}>)
  target_link_options(${PROJECT_NAME}.dbg PUBLIC
                      $<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAGS_TARGET}>)
endif()

# Set custom directory for "install"
install(TARGETS ${PROJECT_NAME} LIBRARY DESTINATION ${INSTALL_LIB_DIR})
if(CMAKE_BUILD_TYPE STREQUAL "Release")
  install(TARGETS ${PROJECT_NAME}.lto LIBRARY DESTINATION ${INSTALL_LIB_DIR})
else()
  install(TARGETS ${PROJECT_NAME}.dbg LIBRARY DESTINATION ${INSTALL_LIB_DIR})
endif()
