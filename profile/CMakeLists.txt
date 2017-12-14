project(profile C)

cmake_minimum_required(VERSION 2.6)

# Generate the lcfg profile shared library.

set(MY_SOURCES profile.c diff.c)

add_library(lcfg_profile SHARED ${MY_SOURCES})

set_target_properties(lcfg_profile PROPERTIES VERSION 1.0.1 SOVERSION 1)

# Cannot have two targets with the same name so the static version has
# '-static' appended and then the name of the output file is set
# separately.

add_library(lcfg_profile-static STATIC ${MY_SOURCES})

set_target_properties(lcfg_profile-static PROPERTIES OUTPUT_NAME "lcfg_profile")

# These next two lines are required but it is unclear exactly what they do.
# The CMake FAQ mentions they are necessary and it does not work otherwise.

SET_TARGET_PROPERTIES(lcfg_profile PROPERTIES CLEAN_DIRECT_OUTPUT 1)
SET_TARGET_PROPERTIES(lcfg_profile-static PROPERTIES CLEAN_DIRECT_OUTPUT 1)

# Also need to link against the lcfg packages and resources libraries

target_link_libraries(lcfg_profile lcfg_packages)
target_link_libraries(lcfg_profile lcfg_resources)
target_link_libraries(lcfg_profile lcfg_utils)

install(TARGETS lcfg_profile lcfg_profile-static
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION ${LIB_INSTALL_DIR}
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR})
