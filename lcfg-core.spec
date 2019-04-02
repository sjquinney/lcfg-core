%global _hardened_build 1
%global build_docs 1
%bcond_without docs

Name:           lcfg-core
Version:        @LCFG_VERSION@
Release:        1
Summary:        LCFG Core libraries
License:        GPLv2
Group:          LCFG/Components
Packager:       Stephen Quinney <squinney@inf.ed.ac.uk>
Source:         %{name}_%{version}.orig.tar.gz
Provides:       lcfg-utils = %{version}-%{release}
BuildRequires:  cmake >= 2.6.0, pkgconfig
BuildRequires:  libxml2-devel
BuildRequires:  bison, flex
%if 0%{?rhel} && 0%{?rhel} < 7
BuildRequires:  db4-devel
%else
BuildRequires:  libdb-devel
%endif
BuildRequires:  rpm-devel

%if 0%{?with_docs}
BuildRequires:  doxygen, doxygen-latex
%endif
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

%if 0%{?with_docs}
%package doc
BuildArch: noarch
Group: LCFG/Devel
Summary: Documentation files for the LCFG core libraries

%description doc
This package contains the documentation for the LCFG core
libraries. The documentation is generated using Doxygen and is
available as a PDF file or as an HTML tree.
%endif

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

%if 0%{?with_docs}
make doc
make -C docs/latex
%endif

%install
rm -rf $RPM_BUILD_ROOT
make install DESTDIR=$RPM_BUILD_ROOT

%if 0%{?with_docs}
mkdir -p $RPM_BUILD_ROOT/usr/share/doc/
ln -s lcfg-core-doc-%{version} $RPM_BUILD_ROOT/usr/share/doc/lcfg-core 
%endif

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
%{_libdir}/liblcfg_common.so.*
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
%{_libdir}/liblcfg_common.a
%{_libdir}/liblcfg_common.so
%{_libdir}/liblcfg_profile.a
%{_libdir}/liblcfg_profile.so
%{_libdir}/liblcfg_packages.a
%{_libdir}/liblcfg_packages.so
%{_libdir}/liblcfg_resources.a
%{_libdir}/liblcfg_resources.so
%{_libdir}/liblcfg_xml.a
%{_libdir}/liblcfg_xml.so

%if 0%{?with_docs}
%files doc
%doc docs/html
%doc docs/latex/refman.pdf
/usr/share/doc/lcfg-core
%endif

%clean
rm -rf $RPM_BUILD_ROOT

%changelog
@LCFG_CHANGELOG@
