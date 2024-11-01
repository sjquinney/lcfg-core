project(xml C)

cmake_minimum_required(VERSION 2.8.12)

# Generate the lcfg profile shared library.

set(MY_SOURCES read.c components.c common.c packages.c resource.c)

add_library(lcfg_xml SHARED ${MY_SOURCES})

set_target_properties(lcfg_xml PROPERTIES VERSION ${PROJECT_VERSION} SOVERSION ${PROJECT_VERSION_MAJOR})

# Cannot have two targets with the same name so the static version has
# '-static' appended and then the name of the output file is set
# separately.

add_library(lcfg_xml-static STATIC ${MY_SOURCES})

set_target_properties(lcfg_xml-static PROPERTIES OUTPUT_NAME "lcfg_xml")

# These next two lines are required but it is unclear exactly what they do.
# The CMake FAQ mentions they are necessary and it does not work otherwise.

SET_TARGET_PROPERTIES(lcfg_xml PROPERTIES CLEAN_DIRECT_OUTPUT 1)
SET_TARGET_PROPERTIES(lcfg_xml-static PROPERTIES CLEAN_DIRECT_OUTPUT 1)

find_package(PkgConfig)
pkg_check_modules(PC_LIBXML QUIET libxml-2.0)
set(LIBXML2_DEFINITIONS ${PC_LIBXML_CFLAGS_OTHER})

find_path(LIBXML2_INCLUDE_DIR libxml/xmlreader.h
          HINTS ${PC_LIBXML_INCLUDEDIR} ${PC_LIBXML_INCLUDE_DIRS}
          PATH_SUFFIXES libxml2 )

find_library(LIBXML2_LIBRARY NAMES xml2 libxml2
             HINTS ${PC_LIBXML_LIBDIR} ${PC_LIBXML_LIBRARY_DIRS} )

set(LIBXML2_LIBRARIES ${LIBXML2_LIBRARY} )
set(LIBXML2_INCLUDE_DIRS ${LIBXML2_INCLUDE_DIR} )

include(FindPackageHandleStandardArgs)

find_package_handle_standard_args(LibXml2  DEFAULT_MSG
                                  LIBXML2_LIBRARY LIBXML2_INCLUDE_DIR)

mark_as_advanced(LIBXML2_INCLUDE_DIR LIBXML2_LIBRARY )

add_definitions(${LIBXML2_DEFINITIONS})

include_directories(${LIBXML2_INCLUDE_DIRS})

target_link_libraries(lcfg_xml ${LIBXML2_LIBRARIES})

# Also need to link against the lcfg profile, packages and resources libraries

target_link_libraries(lcfg_xml lcfg_common)
target_link_libraries(lcfg_xml lcfg_packages)
target_link_libraries(lcfg_xml lcfg_resources)
target_link_libraries(lcfg_xml lcfg_profile)

# Create the pkgspec parser and link it to the pkgtools library.

add_executable(lcfg_xml_reader lcfg_xml_reader.c)

target_link_libraries(lcfg_xml_reader lcfg_xml)
target_link_libraries(lcfg_xml_reader lcfg_bdb)

install(TARGETS lcfg_xml_reader lcfg_xml lcfg_xml-static
        RUNTIME DESTINATION bin
        LIBRARY DESTINATION ${LIB_INSTALL_DIR}
        ARCHIVE DESTINATION ${LIB_INSTALL_DIR})
