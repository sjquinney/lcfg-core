project(bdb C)

cmake_minimum_required(VERSION 2.6)

# Generate the lcfg profile shared library.

set(MY_SOURCES read.c write.c)

add_library(lcfg_bdb SHARED ${MY_SOURCES})

set_target_properties(lcfg_bdb PROPERTIES VERSION 1.0.1 SOVERSION 1)

# Cannot have two targets with the same name so the static version has
# '-static' appended and then the name of the output file is set
# separately.

add_library(lcfg_bdb-static STATIC ${MY_SOURCES})

set_target_properties(lcfg_bdb-static PROPERTIES OUTPUT_NAME "lcfg_bdb")

# These next two lines are required but it is unclear exactly what they do.
# The CMake FAQ mentions they are necessary and it does not work otherwise.

SET_TARGET_PROPERTIES(lcfg_bdb PROPERTIES CLEAN_DIRECT_OUTPUT 1)
SET_TARGET_PROPERTIES(lcfg_bdb-static PROPERTIES CLEAN_DIRECT_OUTPUT 1)

find_library(DB_LIBRARY db)
if(NOT DB_LIBRARY)
  message(FATAL_ERROR "Could not locate db library")
endif(NOT DB_LIBRARY)

target_link_libraries(lcfg_bdb ${DB_LIBRARY})

# Also need to link against the lcfg profile, context and resources libraries

target_link_libraries(lcfg_bdb lcfg_context)
target_link_libraries(lcfg_bdb lcfg_resources)
target_link_libraries(lcfg_bdb lcfg_profile)

# Specify where various files should be installed.

SET(LIB_INSTALL_DIR lib CACHE PATH "Output directory for libraries")

install(TARGETS lcfg_bdb lcfg_bdb-static
        LIBRARY DESTINATION ${LIB_INSTALL_DIR}
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR})
