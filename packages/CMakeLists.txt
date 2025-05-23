project(packagelib C)

cmake_minimum_required(VERSION 2.8.12)

# Generate the packagelib shared library.

set(MY_SOURCES package.c container.c list.c rpm.c deb.c iterator.c set.c setiter.c)

add_library(lcfg_packages SHARED ${MY_SOURCES})

set_target_properties(lcfg_packages PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

# Cannot have two targets with the same name so the static version has
# '-static' appended and then the name of the output file is set
# separately.

add_library(lcfg_packages-static STATIC ${MY_SOURCES})

set_target_properties(lcfg_packages-static PROPERTIES OUTPUT_NAME "lcfg_packages")

# These next two lines are required but it is unclear exactly what they do.
# The CMake FAQ mentions they are necessary and it does not work otherwise.

SET_TARGET_PROPERTIES(lcfg_packages PROPERTIES CLEAN_DIRECT_OUTPUT 1)
SET_TARGET_PROPERTIES(lcfg_packages-static PROPERTIES CLEAN_DIRECT_OUTPUT 1)

# If the rpm library is available then link against it to add extra
# functionality.

find_library(RPM_LIBRARY rpm)
find_library(RPMIO_LIBRARY rpmio)

if(RPM_LIBRARY AND RPMIO_LIBRARY)
add_definitions(-DHAVE_RPMLIB)
target_link_libraries(lcfg_packages ${RPM_LIBRARY} ${RPMIO_LIBRARY})
endif(RPM_LIBRARY AND RPMIO_LIBRARY)

target_link_libraries(lcfg_packages lcfg_common)
target_link_libraries(lcfg_packages lcfg_utils)

# Create the pkgspec parser and link it to the pkgtools library.

add_executable(parse_pkgspec parse_pkgspec.c)
target_link_libraries(parse_pkgspec lcfg_packages)

configure_file( "${CMAKE_CURRENT_SOURCE_DIR}/parse_pkgspec.pod.cin"
                "${CMAKE_CURRENT_BINARY_DIR}/parse_pkgspec.pod" @ONLY)
pod2man("${CMAKE_CURRENT_BINARY_DIR}/parse_pkgspec.pod" ${PROJECT_VERSION} 8 "LCFG/Utils")

add_executable(translaterpmcfg translaterpmcfg.c)
target_link_libraries(translaterpmcfg lcfg_packages)

install(TARGETS parse_pkgspec lcfg_packages lcfg_packages-static
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION ${LIB_INSTALL_DIR}
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR})

install(TARGETS translaterpmcfg DESTINATION sbin)
