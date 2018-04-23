include_directories(${CMAKE_CURRENT_SOURCE_DIR})

find_package(BISON)
if(NOT BISON_FOUND)
  message(FATAL_ERROR "Could not find bison")
endif(NOT BISON_FOUND)

find_package(FLEX)
if(NOT FLEX_FOUND)
  message(FATAL_ERROR "Could not find flex")
endif(NOT FLEX_FOUND)

BISON_TARGET(CtxParser context/parser.y ${CMAKE_CURRENT_BINARY_DIR}/context_parser.c
             COMPILE_FLAGS -Wall)
FLEX_TARGET(CtxScanner context/lexer.l  ${CMAKE_CURRENT_BINARY_DIR}/context_lexer.c)
ADD_FLEX_BISON_DEPENDENCY(CtxScanner CtxParser)

# Generate the lcfg context shared library.

set(MY_SOURCES context/context.c context/list.c context/tools.c context/scanner.c derivation/derivation.c derivation/list.c derivation/map.c farmhash/farmhash.c
               ${BISON_CtxParser_OUTPUTS} ${FLEX_CtxScanner_OUTPUTS} )

add_library(lcfg_common SHARED ${MY_SOURCES})

set_target_properties(lcfg_common PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

# Cannot have two targets with the same name so the static version has
# '-static' appended and then the name of the output file is set
# separately.

add_library(lcfg_common-static STATIC ${MY_SOURCES})

set_target_properties(lcfg_common-static PROPERTIES OUTPUT_NAME "lcfg_common")

# These next two lines are required but it is unclear exactly what they do.
# The CMake FAQ mentions they are necessary and it does not work otherwise.

SET_TARGET_PROPERTIES(lcfg_common PROPERTIES CLEAN_DIRECT_OUTPUT 1)
SET_TARGET_PROPERTIES(lcfg_common-static PROPERTIES CLEAN_DIRECT_OUTPUT 1)

target_link_libraries(lcfg_common lcfg_utils)

install(TARGETS lcfg_common lcfg_common-static
        RUNTIME DESTINATION sbin
        LIBRARY DESTINATION ${LIB_INSTALL_DIR}
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR})
