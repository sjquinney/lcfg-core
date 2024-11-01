cmake_minimum_required(VERSION 2.8.12)

project (lcfg-utils C)

# Generate the lcfgutils shared library.

set(MY_SOURCES libmsg.c utils.c entities.c md5.c slist.c)

add_library(lcfg_utils SHARED ${MY_SOURCES})

set_target_properties(lcfg_utils PROPERTIES VERSION 2.0.1 SOVERSION 2)

# Cannot have two targets with the same name so the static version has
# '-static' appended and then the name of the output file is set
# separately.

add_library(lcfg_utils-static STATIC ${MY_SOURCES})

set_target_properties(lcfg_utils-static PROPERTIES OUTPUT_NAME "lcfg_utils")

# These next two lines are required but it is unclear exactly what they do.
# The CMake FAQ mentions they are necessary and it does not work otherwise.

SET_TARGET_PROPERTIES(lcfg_utils PROPERTIES CLEAN_DIRECT_OUTPUT 1)
SET_TARGET_PROPERTIES(lcfg_utils-static PROPERTIES CLEAN_DIRECT_OUTPUT 1)

# Build and link the various executables

add_executable(daemon daemon.c)

add_executable(lcfgmsg lcfgmsg.c)
target_link_libraries(lcfgmsg lcfg_utils)

add_executable(shiftpressed shiftpressed.c)
target_link_libraries(shiftpressed lcfg_utils)

set(SBIN_TARGETS daemon lcfgmsg shiftpressed)
set(BIN_TARGETS)

# setsid is needed on solaris and macosx

IF(CMAKE_SYSTEM_NAME MATCHES "^(Darwin|SunOS)$")
  SET(REQ_SETSID yes CACHE BOOL "Is the setsid helper required?")
ELSE(CMAKE_SYSTEM_NAME MATCHES "^(Darwin|SunOS)$")
  SET(REQ_SETSID no CACHE BOOL "Is the setsid helper required?")
ENDIF(CMAKE_SYSTEM_NAME MATCHES "^(Darwin|SunOS)$")

if(REQ_SETSID)
  add_executable(setsid setsid.c)
  set(BIN_TARGETS ${BIN_TARGETS} setsid)
  SET(MANFILES ${MANFILES} ${CMAKE_CURRENT_BINARY_DIR}/setsid.8)
endif(REQ_SETSID)

# Specify where various files should be installed.

install(TARGETS ${BIN_TARGETS}
        DESTINATION bin)

install(TARGETS ${SBIN_TARGETS}
        DESTINATION sbin)

install(TARGETS lcfg_utils lcfg_utils-static
        LIBRARY DESTINATION ${LIB_INSTALL_DIR}
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR})

configure_file( "${CMAKE_CURRENT_SOURCE_DIR}/shiftpressed.pod.cin"
                "${CMAKE_CURRENT_BINARY_DIR}/shiftpressed.pod" @ONLY)
pod2man("${CMAKE_CURRENT_BINARY_DIR}/shiftpressed.pod" ${PROJECT_VERSION} 8 "LCFG/Utils")

configure_file( "${CMAKE_CURRENT_SOURCE_DIR}/lcfgmsg.pod.cin"
                "${CMAKE_CURRENT_BINARY_DIR}/lcfgmsg.pod" @ONLY)
pod2man("${CMAKE_CURRENT_BINARY_DIR}/lcfgmsg.pod" ${PROJECT_VERSION} 8 "LCFG/Utils")

configure_file( "${CMAKE_CURRENT_SOURCE_DIR}/lcfgutils.pod.cin"
                "${CMAKE_CURRENT_BINARY_DIR}/lcfgutils.pod" @ONLY)
pod2man("${CMAKE_CURRENT_BINARY_DIR}/lcfgutils.pod" ${PROJECT_VERSION} 8 "LCFG/Utils")

