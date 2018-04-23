cmake_minimum_required(VERSION 2.6)

project (lcfg-core C)

file (STRINGS "lcfg-build-id.txt" PROJECT_VERSION)

# Support for these variables appears in CMake 3.something

string(REPLACE "." ";" VERSION_LIST ${PROJECT_VERSION})
list(GET VERSION_LIST 0 PROJECT_VERSION_MAJOR)
list(GET VERSION_LIST 1 PROJECT_VERSION_MINOR)
list(GET VERSION_LIST 2 PROJECT_VERSION_PATCH)

SET( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wextra -std=gnu99" )

# Specify where various files should be installed.

SET(LIB_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/lib" CACHE PATH "Output directory for libraries")
SET(INCLUDE_INSTALL_DIR "${CMAKE_INSTALL_PREFIX}/include" CACHE PATH "Output directory for header files")

set(LCFGTMP "/var/tmp/lcfg" CACHE PATH "Default directory for temporary files")
set(LCFGLOG "/var/log/lcfg" CACHE PATH "Default directory for log files")

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/include/utils.h.in
               ${CMAKE_CURRENT_BINARY_DIR}/include/utils.h @ONLY)

# Need to look for headers in both source and binary directories, they
# are not always the same location. Anything translated from a *.cin
# file will be found in the binary directory.

include_directories(${CMAKE_BINARY_DIR}/include ${CMAKE_SOURCE_DIR}/include)

link_directories(${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE})

add_subdirectory(utils)

add_subdirectory(common)

add_subdirectory(resources)

add_subdirectory(packages)

add_subdirectory(profile)

add_subdirectory(xml)

add_subdirectory(bdb)

add_subdirectory(pc)

# Headers

install(DIRECTORY ${CMAKE_BINARY_DIR}/include/ include/
                  DESTINATION "${INCLUDE_INSTALL_DIR}/lcfg"
                  FILES_MATCHING PATTERN "*.h"
                  PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)

# Documentation

# add a target to generate API documentation with Doxygen
find_package(Doxygen)
option(BUILD_DOCS "Create and install the HTML based API documentation (requires Doxygen)" ${DOXYGEN_FOUND})

if(BUILD_DOCS)
    if(NOT DOXYGEN_FOUND)
        message(FATAL_ERROR "Doxygen is needed to build the documentation.")
    endif()

    set(doxyfile_in ${CMAKE_CURRENT_SOURCE_DIR}/Doxyfile.in)
    set(doxyfile ${CMAKE_CURRENT_BINARY_DIR}/Doxyfile)

    configure_file(${doxyfile_in} ${doxyfile} @ONLY)

    add_custom_target(doc
        COMMAND ${DOXYGEN_EXECUTABLE} ${doxyfile}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
        COMMENT "Generating API documentation with Doxygen"
        VERBATIM)

endif(BUILD_DOCS)
