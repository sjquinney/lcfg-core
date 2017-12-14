project(context C)

cmake_minimum_required(VERSION 2.6)

include_directories(${CMAKE_SOURCE_DIR}/context)

find_package(BISON)
if(NOT BISON_FOUND)
  message(FATAL_ERROR "Could not find bison")
endif(NOT BISON_FOUND)

find_package(FLEX)
if(NOT FLEX_FOUND)
  message(FATAL_ERROR "Could not find flex")
endif(NOT FLEX_FOUND)

BISON_TARGET(CtxParser parser.y ${CMAKE_CURRENT_BINARY_DIR}/parser.c
             COMPILE_FLAGS -Wall)
FLEX_TARGET(CtxScanner lexer.l  ${CMAKE_CURRENT_BINARY_DIR}/lexer.c)
ADD_FLEX_BISON_DEPENDENCY(CtxScanner CtxParser)

# Generate the lcfg context shared library.

set(MY_SOURCES context.c list.c tools.c scanner.c
               ${BISON_CtxParser_OUTPUTS} ${FLEX_CtxScanner_OUTPUTS} )

add_library(lcfg_context SHARED ${MY_SOURCES})

set_target_properties(lcfg_context PROPERTIES VERSION 1.0.1 SOVERSION 1)

# Cannot have two targets with the same name so the static version has
# '-static' appended and then the name of the output file is set
# separately.

add_library(lcfg_context-static STATIC ${MY_SOURCES})

set_target_properties(lcfg_context-static PROPERTIES OUTPUT_NAME "lcfg_context")

# These next two lines are required but it is unclear exactly what they do.
# The CMake FAQ mentions they are necessary and it does not work otherwise.

SET_TARGET_PROPERTIES(lcfg_context PROPERTIES CLEAN_DIRECT_OUTPUT 1)
SET_TARGET_PROPERTIES(lcfg_context-static PROPERTIES CLEAN_DIRECT_OUTPUT 1)

target_link_libraries(lcfg_context lcfg_utils)

install(TARGETS lcfg_context lcfg_context-static
        RUNTIME DESTINATION sbin
        LIBRARY DESTINATION ${LIB_INSTALL_DIR}
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR})
