# See if the rpm library is available

SET(PACKAGES_DEPS)

find_library(RPM_LIBRARY rpm)
find_library(RPMIO_LIBRARY rpmio)

IF(RPM_LIBRARY AND RPMIO_LIBRARY)
SET(PACKAGES_DEPS ", rpm")
ENDIF(RPM_LIBRARY AND RPMIO_LIBRARY)

# Variable to store output files
SET(outFiles)

# Find all the input files
FILE(GLOB inFiles RELATIVE "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/*.in")

FOREACH(infileName ${inFiles})
    MESSAGE(STATUS "Process file: ${infileName}")

    # Generate output file name
    STRING(REGEX REPLACE ".in\$" "" outfileName "${infileName}")
    SET(outfile "${CMAKE_CURRENT_BINARY_DIR}/${outfileName}")
    MESSAGE(STATUS "Output file: ${outfile}")

    # Generate input file name
    SET(infile "${CMAKE_CURRENT_SOURCE_DIR}/${infileName}")

    CONFIGURE_FILE(${infile} ${outfile} @ONLY)

    # Finally remember the output file for dependencies
    SET(outFiles ${outFiles} "${outfile}")
ENDFOREACH(infileName)

# Setup a target to drive the conversion
ADD_CUSTOM_TARGET(ProcessFiles ALL DEPENDS ${outFiles})

install(FILES ${outFiles} DESTINATION "share/pkgconfig")
