%{!?python_sitearch: %define python_sitearch %(%{__python} -c "from distutils.sysconfig import get_python_lib; print get_python_lib(1)")}

Summary: A translator library for raster geospatial data formats
Name: gdal
Version: 1.3.0
Release: 2
URL: http://gdal.maptools.org/
Source: gdal-%{version}.tar.gz
Patch0: gdal-install.patch
Patch1: gdal-ldflags.patch
License: MIT
Group: System Environment/Libraries

BuildRequires: proj-devel, python-devel, libjpeg-devel
BuildRequires: shapelib-devel, libungif-devel, zlib-devel, libpng-devel
BuildRequires: postgresql-devel
BuildRequires: netcdf-devel
BuildRequires: hdf5-devel, hdf-devel
# This can be included later when geos is available
#BuildRequires: geos-devel
# FC2 bug: unixODBC-devel requires libtool-libs
BuildRequires: unixODBC-devel libtool-libs
BuildRequires: sqlite-devel >= 3.0.0
BuildRequires: mysql-devel
BuildRequires: doxygen

BuildRoot: %{_tmppath}/%{name}-%{version}-%{release}-root-%(%{__id_u} -n)

%package devel
Summary:	Development files for gdal and ogr
Group:		Development/Libraries
Requires:	%{name} = %{version}-%{release}

%package bin
Summary:	Utility programs for gdal and ogr
Group:		Applications/Productivity
Requires:	%{name} = %{version}-%{release}

%package python
Summary:	Python bindings for gdal and ogr
Group:		System Environment/Libraries
Requires:	%{name} = %{version}-%{release}
Requires:	python-abi = %(%{__python} -c "import sys; print sys.version[:3]")

%description
GDAL is a translator library for raster geospatial data formats. As a library,
it presents a single abstract data model to the calling application for all
supported formats. The related OGR library (which lives within the GDAL source
tree) provides a similar capability for simple features vector data.

GDAL supports 40+ popular data formats, including commonly used ones (GeoTIFF,
JPEG, PNG and more) as well as the ones used in GIS and remote sensing software
packages (ERDAS Imagine, ESRI Arc/Info, ENVI, PCI Geomatics). Also supported
many remote sensing and scientific data distribution formats such as HDF, EOS
FAST, NOAA L1B, NetCDF, FITS.

OGR library supports popular vector formats like ESRI Shapefile, TIGER data,
S57, MapInfo File, DGN, GML and more.

%description devel
This package contains libgdal and the appropriate header files.

%description bin
This package contains utility programs, based on GDAL/OGR library, namely
gdal_translate, gdalinfo, gdaladdo, gdalwarp, ogr2ogr, ogrinfo, ogrtindex. 

%description python
This package contains Python bindings for GDAL/OGR library. 

%prep
%setup -q
%patch0 -p0
%patch1 -p1 -b .ldflags

(cd %{_builddir}/%{name}-%{version}; chmod -x frmts/jpeg/gdalexif.h alg/gdal_tps.cpp)

%build

# disable all feature FE does not support, otherwise 
# rebuilding this package will pick up random features
# and make building this rpm non-deterministic.
export CPPFLAGS='-I%{_includedir}/netcdf-3'
export LDFLAGS='-L%{_libdir}/netcdf-3'
%configure \
--includedir=%{_includedir}/gdal \
--datadir=%{_datadir}/gdal \
  --with-grass=no \
  --with-libgrass=no \
  --with-cfitsio=no \
  --with-netcdf=yes \
  --with-png \
  --with-jpeg \
  --with-gif \
  --with-ogdi=no \
  --with-fme=no \
  --with-hdf4=yes \
  --with-hdf5=yes \
  --with-jasper=no \
  --with-ecw=no \
  --with-kakadu=no \
  --with-mrsid=no \
  --without-bsb \
  --with-pg=yes \
  --with-xerces=no \
  --with-odbc \
  --with-oci=no \
  --with-dods-root=no \
  --with-sqlite=yes \
  --with-mysql=yes \
  --with-geos=no

cp GDALmake.opt GDALmake.opt.orig
sed -e "s/^CFLAGS.*$/CFLAGS=$CFLAGS/" \
 -e "s/^CXXFLAGS.*$/CXXFLAGS=$CXXFLAGS/" \
 -e "s/^FFLAGS.*$/FFLAGS=$FFLAGS/" \
 GDALmake.opt.orig > GDALmake.opt

make  %{?_smp_mflags}

make docs
find html -name .cvsignore -exec rm -f {} \;
unset CPPFLAGS
unset LDFLAGS

%install
rm -rf %{buildroot}

%makeinstall \
 INST_PYMOD=%{buildroot}%{python_sitearch} \
 INST_MAN=%{buildroot}/%{_mandir} \
 INST_LIB=%{buildroot}/%{_libdir} \
 INST_INCLUDE=%{buildroot}/%{_includedir}/gdal \
 INST_BIN=%{buildroot}/%{_bindir} \
 INST_DATA=%{buildroot}/%{_datadir}/gdal

%post -p /sbin/ldconfig

%postun -p /sbin/ldconfig

%clean
rm -rf %{buildroot}

%files 
%defattr(-,root,root)
%doc ChangeLog VERSION NEWS HOWTO-RELEASE
%{_libdir}/lib*.so.*
%{_datadir}/gdal

%files devel
%defattr(-,root,root,-)
%doc html
%{_bindir}/gdal-config
%{_includedir}/*
%{_libdir}/*.a
%{_libdir}/*.la
%{_libdir}/libgdal.so
%{_mandir}/man1/gdal-config*

%files bin
%defattr(-,root,root,-)
%{_bindir}/*
%exclude %{_bindir}/gdal-config
%exclude %{_bindir}/*.py
%{_mandir}/man1/*
%exclude %{_mandir}/man1/gdal-config*

%files python
%defattr(-,root,root,-)
%{python_sitearch}
%{_bindir}/*.py
%exclude %{_libdir}/python*/site-packages/*.a

%changelog
* Tue Sep 20 2005 Silke Reimer <silke@intevation.de> 1.3.0-2
- made frmts/jpeg/gdalexif.h alg/gdal_tps.cpp not executable thus fixing
  rpmlint error "gdal-debuginfo script-without-shellbang"
- using external libtiff
- explicit requireing mysql, hdf and hdf5 support thus avoiding non 
  deterministic build results
- moved python scripts from gdal-bin to gdal-python

* Thu Aug 25 2005 Silke Reimer <silke@intevation.de> 1.3.0-1
- new upstream version
- add sqlite support

* Sat Dec 18 2004 Various <http://fedora.us/> - 1.2.5-0.fdr.1
- add python-abi dependency to python sub-package
- install python stuff into python sitearch
- add patch to prepend LDFLAGS when linking
- fetch NetCDF from its own lib/include directories

* Fri Oct 01 2004 Ralf Corsepius <ralf@links2linux.de> 0:1.2.3-0.fdr.3
- install headers to %%{_includedir}/gdal.
- install data to %%{_datadir}/gdal.
- disable geos.
- build docs, install them into *-devel.
- Fundamental spec-file cleanup.

* Wed Sep 29 2004 Silke Reimer <silke@intevation.net> 0:1.2.3-0.fdr.2
- new upstream version

* Mon Sep 27 2004 Silke Reimer <silke@intevation.net> 0:1.2.2-0.fdr.2
- new upstream version
- make use of $RPM_OPT_FLAGS by patching GDALmake.opt
- explicit set of _prefix has been dropped

* Tue Aug 31 2004 Silke Reimer <silke.reimer@intevation.net> 0:1.2.1-1.fdr.2
- Changed BuildRoot according to standard specfile.
- Added %{?_smp_mflags} to make
- Changed post and postun to avoid printing and additional requirements for 
  post and postun
- Changed Group of main package to "System Environment/Libraries"
- Use %configure-macro 

* Fri Aug 06 2004 Silke Reimer <silke.reimer@intevation.net> - 0:1.2.1-0.fdr.2
- substantial changes to fit to Fedora naming conventions

* Wed Jun 30 2004 Silke Reimer <silke.reimer@intevation.net> - 1.2.1-0
- Initial build
