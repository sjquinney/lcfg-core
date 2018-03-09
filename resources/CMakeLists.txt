project(resourcelib C)

cmake_minimum_required(VERSION 2.6)

# Generate the resourcelib shared library.

set(MY_SOURCES components/component.c components/set.c components/diff.c resource.c tags/tag.c tags/list.c tags/iterator.c templates.c iterator.c mutate.c diff.c)

add_library(lcfg_resources SHARED ${MY_SOURCES})

set_target_properties(lcfg_resources PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

# Cannot have two targets with the same name so the static version has
# '-static' appended and then the name of the output file is set
# separately.

add_library(lcfg_resources-static STATIC ${MY_SOURCES})

set_target_properties(lcfg_resources-static PROPERTIES OUTPUT_NAME "lcfg_resources")

# These next two lines are required but it is unclear exactly what they do.
# The CMake FAQ mentions they are necessary and it does not work otherwise.

SET_TARGET_PROPERTIES(lcfg_resources PROPERTIES CLEAN_DIRECT_OUTPUT 1)
SET_TARGET_PROPERTIES(lcfg_resources-static PROPERTIES CLEAN_DIRECT_OUTPUT 1)

target_link_libraries(lcfg_resources lcfg_common)
target_link_libraries(lcfg_resources lcfg_utils)

install(TARGETS lcfg_resources lcfg_resources-static
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION ${LIB_INSTALL_DIR}
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR})
