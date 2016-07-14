project(context C)

cmake_minimum_required(VERSION 2.6)

# Generate the lcfg context shared library.

set(MY_SOURCES context.c  ctxlist.c  setctx.c  tools.c)

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

# Create the setctx tool and link it to the lcfg_context library.

add_executable(setctx setctx.c)

target_link_libraries(setctx lcfg_context)

# Specify where various files should be installed.

SET(LIB_INSTALL_DIR lib CACHE PATH "Output directory for libraries")

install(TARGETS setctx lcfg_context lcfg_context-static
        RUNTIME DESTINATION sbin
        LIBRARY DESTINATION ${LIB_INSTALL_DIR}
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR})
