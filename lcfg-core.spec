Name:           lcfg-core
Version:        @LCFG_VERSION@
Release:        @LCFG_RELEASE@
Summary:        LCFG Core libraries
License:        GPLv2
Group:          LCFG/Components
Packager:       Stephen Quinney <squinney@inf.ed.ac.uk>
Source:         %{name}-%{version}.tar.gz
Provides:       lcfg-utils
BuildRequires:  cmake >= 2.6.0, pkgconfig
BuildRequires:  libxml2-devel
BuildRequires:  bison, flex
%if 0%{?rhel} && 0%{?rhel} < 7
BuildRequires:  db4-devel
%else
BuildRequires:  libdb-devel
%endif
BuildRequires:  rpm-devel
BuildRequires:  doxygen, doxygen-latex
BuildRoot:      %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)
Requires(post,postun):         /sbin/ldconfig

%description
This provides all the core C libraries for the LCFG configuration
management framework. For more information on the LCFG project see the
website at http://www.lcfg.org/ Included is support for processing
LCFG package and resource data to/from various file formats.

%package devel
Group: LCFG/Devel
Summary: Development files for the LCFG core libraries
Requires: %{name} = %{version}-%{release}
Provides: lcfg-utils-devel

%description devel
This package contains the libraries and header files necessary for
building software and linking against the LCFG core libraries.

%package doc
Group: LCFG/Devel
Summary: Documentation files for the LCFG core libraries

%description doc
This package contains the documentation for the LCFG core
libraries. The documentation is generated using Doxygen and is
available as a PDF file or as an HTML tree.

%prep
%setup

%if 0%{?rhel} && 0%{?rhel} <= 7
%cmake -DLCFGLOG:STRING=/var/lcfg/log -DLCFGTMP:STRING=/var/lcfg/tmp
%else
%cmake
%endif

%build
make

# Documentation

make doc
make -C docs/latex

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT
mkdir -p $RPM_BUILD_ROOT/usr/share/doc/
ln -s lcfg-core-doc-%{version} $RPM_BUILD_ROOT/usr/share/doc/lcfg-core 

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%files
%defattr(-,root,root)
%doc ChangeLog README.md
#%{_mandir}/man1/*
%{_mandir}/man8/*
%{_bindir}/parse_pkgspec
%{_sbindir}/daemon
%{_sbindir}/lcfgmsg
%{_sbindir}/shiftpressed
%{_sbindir}/translaterpmcfg
%{_bindir}/lcfg_xml_reader
%{_libdir}/liblcfg_bdb.so.*
%{_libdir}/liblcfg_utils.so.*
%{_libdir}/liblcfg_context.so.*
%{_libdir}/liblcfg_profile.so.*
%{_libdir}/liblcfg_packages.so.*
%{_libdir}/liblcfg_resources.so.*
%{_libdir}/liblcfg_xml.so.*

%files devel
%{_datadir}/pkgconfig/*.pc
%{_includedir}/lcfg/*.h
%{_libdir}/liblcfg_bdb.a
%{_libdir}/liblcfg_bdb.so
%{_libdir}/liblcfg_utils.a
%{_libdir}/liblcfg_utils.so
%{_libdir}/liblcfg_context.a
%{_libdir}/liblcfg_context.so
%{_libdir}/liblcfg_profile.a
%{_libdir}/liblcfg_profile.so
%{_libdir}/liblcfg_packages.a
%{_libdir}/liblcfg_packages.so
%{_libdir}/liblcfg_resources.a
%{_libdir}/liblcfg_resources.so
%{_libdir}/liblcfg_xml.a
%{_libdir}/liblcfg_xml.so

%files doc
%doc docs/html
%doc docs/latex/refman.pdf
/usr/share/doc/lcfg-core

%clean
rm -rf $RPM_BUILD_ROOT

%changelog
@LCFG_CHANGELOG@
