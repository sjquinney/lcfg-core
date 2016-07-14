cmake_minimum_required(VERSION 2.6)

project (lcfg-core C)

include(lcfg.cmake)

lcfg_translate()

SET( CMAKE_C_FLAGS  "${CMAKE_C_FLAGS} -Wextra" )

# Need to look for headers in both source and binary directories, they
# are not always the same location. Anything translated from a *.cin
# file will be found in the binary directory.

include_directories(${CMAKE_BINARY_DIR}/include ${CMAKE_SOURCE_DIR}/include)

link_directories(${CMAKE_BINARY_DIR}/lib/${CMAKE_BUILD_TYPE})

add_subdirectory(utils)

add_subdirectory(context)

add_subdirectory(resources)

add_subdirectory(packages)

add_subdirectory(profile)

add_subdirectory(xml)

add_subdirectory(bdb)

# Headers

install(DIRECTORY ${CMAKE_BINARY_DIR}/include/ include/
                  DESTINATION "include/lcfg"
                  FILES_MATCHING PATTERN "*.h"
                  PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)

# pkg-config

install(DIRECTORY ${CMAKE_BINARY_DIR}/pc/ pc/
                  DESTINATION "share/pkgconfig"
                  FILES_MATCHING PATTERN "*.pc"
                  PERMISSIONS OWNER_READ GROUP_READ WORLD_READ)

# Documentation

lcfg_add_pod(${CMAKE_BINARY_DIR}/docs/parse_pkgspec.pod docs/parse_pkgspec 1)
lcfg_add_pod(${CMAKE_BINARY_DIR}/docs/lcfgpackagelib.pod docs/lcfgpackagelib 3)
lcfg_add_pod(${CMAKE_BINARY_DIR}/docs/lcfgpackageiterator.pod docs/lcfgpackageiterator 3)
lcfg_add_pod(${CMAKE_BINARY_DIR}/docs/lcfgpackagespec.pod docs/lcfgpackagespec 3)
lcfg_add_pod(${CMAKE_BINARY_DIR}/docs/lcfgpackagenode.pod docs/lcfgpackagenode 3)
