# LCFG Core Libraries

This provides all the core C libraries for the LCFG configuration
management framework. For more information on the LCFG project see the
website at http://www.lcfg.org/

It is expected that these libraries will build and function correctly
on any Unix-like system (including MacOSX) but they have only been
tested on Redhat Enterprise/Fedora and Debian/Ubuntu systems.

## Build and Install

    tar zxf lcfg-core.tar.gz
    cd lcfg-core
    mkdir build
    cd build
    cmake ..
    make
    make doc
    sudo make install

Debugging can be enabled by passing the -DCMAKE_BUILD_TYPE=Debug
option on the cmake command line.

## Requirements

To build the software a C compiler is required. The code has been
tested with various versions of gcc from 4.8.5 through to 6.3.0. The
code can also alternatively be compiled using clang (versions from 3.8
through to 5.0).

The build process is managed using CMake - https://cmake.org/ - which
in turn will typically use Make.

The development packages for Berkeley DB and libxml2 are also required.

### Redhat Enterprise (and also Centos, Scientific Linux, Oracle)

On EL7 install the build tools with:

    yum install cmake make pkg-config gcc gcc-c++

Install the development library packages with:

    yum install rpm-devel libxml2-devel libdb-devel

To generate the documentation the Doxygen packages are required:

    yum install doxygen-latex doxygen

### Debian/Ubuntu

    apt install build-essential cmake pkg-config g++

    apt install libdb-dev libxml2-dev

You can also optionally install the librpm-dev package to add support
for RPM package versions but it's not essential.

To generate the documentation the Doxygen packages are required:

    apt install doxygen-latex doxygen

## See Also

There are Perl bindings available for the LCFG core libraries. Search
for LCFG::Core on http://www.cpan.org/

## Bugs and Limitations

Please report any bugs or problems (or praise!) to bugs@lcfg.org,
feedback and patches are also always very welcome.

## Author

Stephen Quinney <squinney@inf.ed.ac.uk>

## License and Copyright

Copyright (C) 2013-2017 University of Edinburgh. All rights reserved.

This library is free software; you can redistribute it and/or modify
it under the terms of the GPL, version 2 or later.

