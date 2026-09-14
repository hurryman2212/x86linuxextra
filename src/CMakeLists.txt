# Enlist source from current directory recursively
file(GLOB_RECURSE PROJECT_SRCS CONFIGURE_DEPENDS *.c)

# Add external library to link
set(PROJECT_LIBRARIES Threads::Threads ${CMAKE_DL_LIBS})

# Set project-specific build options
option(NO_UMWAIT "Disable UMWAIT support" OFF)
option(FORCE_UMWAIT "Require UMWAIT support" OFF)
if(NO_UMWAIT AND FORCE_UMWAIT)
  message(
    FATAL_ERROR "Both `NO_UMWAIT` and `FORCE_UMWAIT` defined! Aborting...")
endif()
if(NO_UMWAIT)
  set(C_FLAGS_PROJECT ${C_FLAGS_PROJECT} -D_NO_UMWAIT)
else()
  set(C_FLAGS_PROJECT ${C_FLAGS_PROJECT} -mwaitpkg)
endif()
if(FORCE_UMWAIT)
  set(C_FLAGS_PROJECT ${C_FLAGS_PROJECT} -D_FORCE_UMWAIT)
endif()
set(CXX_FLAGS_PROJECT ${C_FLAGS_PROJECT})

# DO NOT EDIT THE BELOW!

# Declare project for this scope
project(lib${ROOT_PROJECT_NAME})

# Declare object output(s) to build with source
add_library(${PROJECT_NAME} OBJECT ${PROJECT_SRCS})
add_dependencies(${PROJECT_NAME} version-${ROOT_PROJECT_NAME})

# Add project-specific compiler options
target_compile_options(${PROJECT_NAME}
                       PRIVATE $<$<COMPILE_LANGUAGE:C>:${C_FLAGS_PROJECT}>)
target_compile_options(${PROJECT_NAME}
                       PRIVATE $<$<COMPILE_LANGUAGE:CXX>:${CXX_FLAGS_PROJECT}>)

# Link external libraries we need
if(PROJECT_LIBRARIES)
  target_link_libraries(${PROJECT_NAME} PRIVATE ${PROJECT_LIBRARIES})
endif()

# Resolve project headers relative to the including file or public include path.
target_include_directories(
  ${PROJECT_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../include"
                          "${CMAKE_CURRENT_BINARY_DIR}/..")
target_compile_definitions(${PROJECT_NAME} PRIVATE _GNU_SOURCE)
target_link_libraries(${PROJECT_NAME}
                      PRIVATE x86linuxextra::_libbacktrace_headers)
if(NO_UMWAIT)
  target_compile_definitions(headers INTERFACE _NO_UMWAIT)
elseif(FORCE_UMWAIT)
  target_compile_definitions(headers INTERFACE _FORCE_UMWAIT)
endif()

# Add `-fpic` compiler option for compiling objects
target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:C>:-fpic>)
target_compile_options(${PROJECT_NAME} PRIVATE $<$<COMPILE_LANGUAGE:CXX>:-fpic>)

# Declare project static and shared libraries to link with the compiled objects
add_library(${PROJECT_NAME}.a STATIC $<TARGET_OBJECTS:${PROJECT_NAME}>)
add_library(${PROJECT_NAME}.so SHARED $<TARGET_OBJECTS:${PROJECT_NAME}>)
foreach(kind a so)
  add_library(x86linuxextra::${PROJECT_NAME}.${kind} ALIAS
              ${PROJECT_NAME}.${kind})
  target_link_libraries(
    ${PROJECT_NAME}.${kind}
    PUBLIC x86linuxextra::headers
    PRIVATE ${PROJECT_LIBRARIES})
endforeach()
target_link_libraries(${PROJECT_NAME}.a
                      PRIVATE x86linuxextra::_libbacktrace_static)
target_link_libraries(${PROJECT_NAME}.so
                      PRIVATE x86linuxextra::_libbacktrace_static)

# Enforce output name format
set_target_properties(${PROJECT_NAME} PROPERTIES PREFIX "" OUTPUT_NAME
                                                           ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME}.a PROPERTIES PREFIX "" OUTPUT_NAME
                                                             ${PROJECT_NAME})
set_target_properties(${PROJECT_NAME}.so PROPERTIES PREFIX "" OUTPUT_NAME
                                                              ${PROJECT_NAME})

# Set custom directory for "install"
install(
  TARGETS ${PROJECT_NAME}.a ${PROJECT_NAME}.so headers
  EXPORT x86linuxextraTargets
  ARCHIVE DESTINATION "${CMAKE_INSTALL_LIBDIR}"
  LIBRARY DESTINATION "${CMAKE_INSTALL_LIBDIR}")
