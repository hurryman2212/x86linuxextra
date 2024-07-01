# Enlist source from current directory recursively
file(GLOB_RECURSE PROJECT_SOURCES *.c)

# Add external library to link
find_library(libbacktrace backtrace)
set(PROJECT_LIBRARIES ${libbacktrace})

# Set project-specific build options
if(NO_UMWAIT STREQUAL "yes" AND FORCE_UMWAIT STREQUAL "yes")
  message(
    FATAL_ERROR "Both `NO_UMWAIT` and `FORCE_UMWAIT` defined! Aborting...")
endif()
if(NO_UMWAIT STREQUAL "yes")
  set(C_FLAGS_PROJECT ${C_FLAGS_PROJECT} -D_NO_UMWAIT)
else()
  set(C_FLAGS_PROJECT ${C_FLAGS_PROJECT} -mwaitpkg)
endif()
if(FORCE_UMWAIT STREQUAL "yes")
  set(C_FLAGS_PROJECT ${C_FLAGS_PROJECT} -D_FORCE_UMWAIT)
endif()
set(CXX_FLAGS_PROJECT ${C_FLAGS_PROJECT})

# DO NOT EDIT THE BELOW!

# Declare project for this scope
project(lib${ROOT_PROJECT_NAME})

# Declare object output(s) to build with source
add_library(${PROJECT_NAME} OBJECT ${PROJECT_SOURCES})

# Add project-specific compiler options
target_compile_options(${PROJECT_NAME}
                       PRIVATE $<$<COMPILE_LANGUAGE:C>:${C_FLAGS_PROJECT}>)
target_compile_options(${PROJECT_NAME}
                       PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAGS_PROJECT}>)

# Link external libraries we need
if(PROJECT_LIBRARIES)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_LIBRARIES})
endif()

# Include directory for headers
target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

# Add `-fpic` compiler option for compiling objects
target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:C>:-fpic>)
target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fpic>)

# Declare project static and shared libraries to link with the compiled objects
add_library(${PROJECT_NAME}.a STATIC)
target_link_libraries(${PROJECT_NAME}.a PRIVATE ${PROJECT_NAME})
add_library(${PROJECT_NAME}.so SHARED)
target_link_libraries(${PROJECT_NAME}.so PRIVATE ${PROJECT_NAME})

# Enforce output name format
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" OUTPUT_NAME
                                                           ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME}.a PROPERTIES PREFIX "" OUTPUT_NAME
                                                             ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME}.so PROPERTIES PREFIX "" OUTPUT_NAME
                                                              ${PROJECT_NAME})

# Set custom directory for "install"
install(TARGETS ${PROJECT_NAME}.a LIBRARY)
install(TARGETS ${PROJECT_NAME}.so LIBRARY)
