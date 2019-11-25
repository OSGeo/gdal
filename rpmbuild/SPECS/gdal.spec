#TODO: g2clib and grib (said to be modified)
#TODO: Python 3 modules should be possible since 1.7
#TODO: Create script to make clean tarball
#TODO: msg needs to have PublicDecompWT.zip from EUMETSAT, which is not free;
#      Building without msg therefore
#TODO: e00compr bundled?
#TODO: There are tests for bindings -- at least for Perl
#TODO: Java has a directory with test data and a build target called test
#      It uses %%{JAVA_RUN}; make test seems to work in the build directory
#TODO: e00compr source is the same in the package and bundled in GDAL
#TODO: Consider doxy patch from Suse, setting EXTRACT_LOCAL_CLASSES  = NO

# Soname should be bumped on API/ABI break
# http://trac.osgeo.org/gdal/ticket/4543

# Conditionals and structures for EL 5 are there
# to make life easier for downstream ELGIS.
# Sadly noarch doesn't work in EL 5, see
# http://fedoraproject.org/wiki/EPEL/GuidelinesAndPolicies

# He also suggest to use --with-static-proj4 to actually link to proj, instead of dlopen()ing it.

# Major digit of the proj so version
%global proj_somaj 7

# Tests can be of a different version
%global testversion 2.4.2
%global run_tests 0

%global bashcompletiondir %(pkg-config --variable=compatdir bash-completion)

%if 0%{?bootstrap}
%global build_refman 0
%global with_mysql 0
%global mysql --without-mysql
%global with_poppler 0
%global poppler --without-poppler
%global with_spatialite 0
%global spatialite --without-spatialite
%else
# Enable/disable generating refmans
# texlive currently broken deps and FTBFS in rawhide
%global build_refman 1
# https://bugzilla.redhat.com/show_bug.cgi?id=1490492
%global with_mysql 1
%global mysql --with-mysql
# https://bugzilla.redhat.com/show_bug.cgi?id=1490492
%global with_poppler 1
%global poppler --with-poppler
%global with_spatialite 1
%global spatialite "--with-spatialite"
%endif

# No ppc64 build for spatialite in EL6
# https://bugzilla.redhat.com/show_bug.cgi?id=663938
%if 0%{?rhel} == 6
%ifnarch ppc64
%global with_spatialite 0
%global spatialite --without-spatialite
%endif
%endif

Name:		gdal
Version:	3.0.2.hdf5r.1
Release:	1%{?dist}%{?bootstrap:.%{bootstrap}.bootstrap}
Summary:	GIS file format library
Group:		System Environment/Libraries
License:	MIT
URL:		http://www.gdal.org
# Source0:   http://download.osgeo.org/gdal/%%{version}/gdal-%%{version}.tar.xz
# See PROVENANCE.TXT-fedora and the cleaner script for details!

Source0:	%{name}-%{version}.tar.gz
#Source1:	http://download.osgeo.org/%{name}/%{testversion}/%{name}autotest-%{testversion}.tar.gz
#Source1:	%{name}.pom

# Cleaner script for the tarball
#Source3:	%{name}-cleaner.sh

#Source4:	PROVENANCE.TXT-fedora

# Patch to use system g2clib
#Patch1:		%{name}-g2clib.patch
# Patch for Fedora JNI library location
#Patch2:		%{name}-jni.patch
# Fix bash-completion install dir
#Patch3:		%{name}-completion.patch

# Fedora uses Alternatives for Java
#Patch8:		%{name}-1.9.0-java.patch

#Patch9:		%{name}-2.2.2-zlib.patch

BuildRequires:	doxygen
BuildRequires:	expat-devel
BuildRequires:	fontconfig-devel
# No freexl in EL5
BuildRequires:	hdf5-devel

# No libwebp in EL 5 and 6
#BuildRequires:	proj-devel

# Run time dependency for gpsbabel driver
Requires:	gpsbabel

#Requires:	%{name}-libs%{?_isa} = %{version}-%{release}

# We have multilib triage
%if "%{_lib}" == "lib"
  %global cpuarch 32
%else
  %global cpuarch 64
%endif

%if ! (0%{?fedora} || 0%{?rhel} > 5)
%{!?python_sitelib: %global python_sitelib %(%{__python} -c "from distutils.sysconfig import get_python_lib; print(get_python_lib())")}
%{!?python_sitearch: %global python_sitearch %(%{__python} -c "from distutils.sysconfig import get_python_lib; print(get_python_lib(1))")}
%endif

#TODO: Description on the lib?
%description
Geospatial Data Abstraction Library (GDAL/OGR) is a cross platform
C++ translator library for raster and vector geospatial data formats.
As a library, it presents a single abstract data model to the calling
application for all supported formats. It also comes with a variety of
useful commandline utilities for data translation and processing.

It provides the primary data access engine for many applications.
GDAL/OGR is the most widely used geospatial data access library.


%package devel
Summary:	Development files for the GDAL file format library
Group:	Development/Libraries

# Old rpm didn't figure out
%if 0%{?rhel} < 6
Requires: pkgconfig
%endif

Requires:	%{name}-libs%{?_isa} = %{version}-%{release}
Obsoletes:	%{name}-static < 1.9.0-1

%description devel
This package contains development files for GDAL.


%package libs
Summary:	GDAL file format library
Group:		System Environment/Libraries
# https://trac.osgeo.org/gdal/ticket/3978#comment:5
Obsoletes:	%{name}-ruby < 1.11.0-1

%description libs
This package contains the GDAL file format library.


%package java
Summary:	Java modules for the GDAL file format library
Group:		Development/Libraries
Requires:	jpackage-utils
Requires:	%{name}-libs%{?_isa} = %{version}-%{release}

%description java
The GDAL Java modules provide support to handle multiple GIS file formats.


%package javadoc
Summary:	Javadocs for %{name}
Group:		Documentation
Requires:	jpackage-utils
BuildArch:	noarch

%description javadoc
This package contains the API documentation for %{name}.


%package perl
Summary:	Perl modules for the GDAL file format library
Group:		Development/Libraries
Requires:	%{name}-libs%{?_isa} = %{version}-%{release}
Requires:	perl(:MODULE_COMPAT_%(eval "`%{__perl} -V:version`"; echo $version))

%description perl
The GDAL Perl modules provide support to handle multiple GIS file formats.


%package -n python2-gdal
%{?python_provide:%python_provide python2-gdal}
# Remove before F30
Provides: %{name}-python = %{version}-%{release}
Provides: %{name}-python%{?_isa} = %{version}-%{release}
Obsoletes: %{name}-python < %{version}-%{release}
Summary:	Python modules for the GDAL file format library
Group:		Development/Libraries
Requires:	numpy
Requires:	%{name}-libs%{?_isa} = %{version}-%{release}

%description -n python2-gdal
The GDAL Python modules provide support to handle multiple GIS file formats.
The package also includes a couple of useful utilities in Python.


%package python3
Summary:	Python modules for the GDAL file format library
Group:		Development/Libraries
Requires:	python3-numpy
Requires:	%{name}-libs%{?_isa} = %{version}-%{release}

%description python3
The GDAL Python 3 modules provide support to handle multiple GIS file formats.


#%package doc
#Summary:	Documentation for GDAL
#Group:		Documentation
#BuildArch:	noarch

#%description doc
#This package contains HTML and PDF documentation for GDAL.

# We don't want to provide private Python extension libs
#%global __provides_exclude_from ^(%{python2_sitearch}|%{python3_sitearch})/.*\.so$

%prep
%setup -q -n %{name}-%{version}

#%patch1 -p1 -b .g2clib~
#%patch2 -p1 -b .jni~
#%patch3 -p1 -b .completion~
#%patch8 -p1 -b .java~
#%patch9 -p1 -b .zlib~

# Copy in PROVENANCE.TXT-fedora
#cp -p %SOURCE4 .

# Sanitize linebreaks and encoding
#TODO: Don't touch data directory!
# /frmts/grib/degrib18/degrib/metaname.cpp
# and geoconcept.c are potentially dangerous to change
set +x
for f in `find . -type f` ; do
  if file $f | grep -q ISO-8859 ; then
    set -x
    iconv -f ISO-8859-1 -t UTF-8 $f > ${f}.tmp && \
      mv -f ${f}.tmp $f
    set +x
  fi
  if file $f | grep -q CRLF ; then
    set -x
    sed -i -e 's|\r||g' $f
    set +x
  fi
done
set -x

# Fix mandir
sed -i "s|^mandir=.*|mandir='\${prefix}/share/man'|" configure

%build
[ -e /opt/rh/devtoolset-8/enable ] && . /opt/rh/devtoolset-8/enable
#TODO: Couldn't I have modified that in the prep section?
%ifarch sparcv9 sparc64 s390 s390x
export CFLAGS="$RPM_OPT_FLAGS -fPIC"
%else
export CFLAGS="$RPM_OPT_FLAGS -fpic"
%endif
#export CXXFLAGS="$CFLAGS -I%{_includedir}/libgeotiff -I%{_includedir}/tirpc"
#export CPPFLAGS="$CPPFLAGS -I%{_includedir}/libgeotiff -I%{_includedir}/tirpc"

# For future reference:
# epsilon: Stalled review -- https://bugzilla.redhat.com/show_bug.cgi?id=660024
# Building without pgeo driver, because it drags in Java

#%if 0%{?fedora} >= 27 || 0%{?rhel} > 7
#%global g2clib g2c_v1.6.0
#%else
#%global g2clib grib2c
#%endif

%configure \
	--datadir=%{_datadir}/%{name}/ \
	--includedir=%{_includedir}/%{name}/ \
	--prefix=%{_prefix}	\
	--with-hdf5 \
        --with-hdf4 \
	--without-jpeg12 \
	--without-mdb \
	--with-netcdf \
	--without-msg \
	--with-png \
	--with-threads \
        --with-proj=/usr/local/proj6 \
%if 0%{?rhel} > 6
        --with-xerces \
%endif
	--enable-shared	\
	--with-libkml \
        --with-python

POPPLER_OPTS="POPPLER_0_20_OR_LATER=yes POPPLER_0_23_OR_LATER=yes POPPLER_BASE_STREAM_HAS_TWO_ARGS=yes"
%if 0%{?fedora} > 26 || 0%{?rhel} > 7
POPPLER_OPTS="$POPPLER_OPTS POPPLER_0_58_OR_LATER=yes"
%endif

make -j8 %{?_smp_mflags} $POPPLER_OPTS

make man
make docs

%install
rm -rf %{buildroot}

make    DESTDIR=%{buildroot}    \
        install \
        install-man

# Directory for auto-loading plugins
mkdir -p %{buildroot}%{_libdir}/%{name}plugins

#TODO: Header date lost during installation
# Install multilib cpl_config.h bz#430894
install -p -D -m 644 port/cpl_config.h %{buildroot}%{_includedir}/%{name}/cpl_config-%{cpuarch}.h

#>>>>>>>>>>>>>
cat > %{buildroot}%{_includedir}/%{name}/cpl_config.h <<EOF
#include <bits/wordsize.h>

#if __WORDSIZE == 32
#include "gdal/cpl_config-32.h"
#else
#if __WORDSIZE == 64
#include "gdal/cpl_config-64.h"
#else
#error "Unknown word size"
#endif
#endif
EOF
#<<<<<<<<<<<<<
touch -r NEWS port/cpl_config.h

# Create and install pkgconfig file
#TODO: Why does that exist? Does Grass really use it? I don't think so.
# http://trac.osgeo.org/gdal/ticket/3470
#>>>>>>>>>>>>>
cat > %{name}.pc <<EOF
prefix=%{_prefix}
exec_prefix=%{_prefix}
libdir=%{_libdir}
includedir=%{_includedir}

Name: GDAL
Description: GIS file format library
Version: %{version}
Libs: -L\${libdir} -lgdal
Cflags: -I\${includedir}/%{name}
EOF
#<<<<<<<<<<<<<
mkdir -p %{buildroot}%{_libdir}/pkgconfig/
install -m 644 %{name}.pc %{buildroot}%{_libdir}/pkgconfig/
touch -r NEWS %{buildroot}%{_libdir}/pkgconfig/%{name}.pc

# Multilib gdal-config
# Rename the original script to gdal-config-$arch (stores arch-specific information)
# and create a script to call one or the other -- depending on detected architecture
# TODO: The extra script will direct you to 64 bit libs on
# 64 bit systems -- whether you like that or not
mv %{buildroot}%{_bindir}/%{name}-config %{buildroot}%{_bindir}/%{name}-config-%{cpuarch}
#>>>>>>>>>>>>>
cat > %{buildroot}%{_bindir}/%{name}-config <<EOF
#!/bin/bash

ARCH=\$(uname -m)
case \$ARCH in
x86_64 | ppc64 | ppc64le | ia64 | s390x | sparc64 | alpha | alphaev6 | aarch64 )
%{name}-config-64 \${*}
;;
*)
%{name}-config-32 \${*}
;;
esac
EOF
#<<<<<<<<<<<<<
touch -r NEWS %{buildroot}%{_bindir}/%{name}-config
chmod 755 %{buildroot}%{_bindir}/%{name}-config

# Clean up junk
rm -f %{buildroot}%{_bindir}/*.dox

#jni-libs and libgdal are also built static (*.a)
#.exists and .packlist stem from Perl
for junk in {*.a,*.la,*.bs,.exists,.packlist} ; do
  find %{buildroot} -name "$junk" -exec rm -rf '{}' \;
done

# Don't duplicate license files
rm -f %{buildroot}%{_datadir}/%{name}/LICENSE.TXT

# Throw away random API man mages plus artefact seemingly caused by Doxygen 1.8.1 or 1.8.1.1
for f in 'GDAL*' pct2rgb rgb2pct BandProperty ColorAssociation CutlineTransformer DatasetProperty EnhanceCBInfo ListFieldDesc NamedColor OGRSplitListFieldLayer VRTBuilder; do
  rm -rf %{buildroot}%{_mandir}/man1/$f.1*
done

# Cleanup .pyc for now
rm -f %{buildroot}%{_bindir}/*.pyc

#TODO: What's that?
rm -f %{buildroot}%{_mandir}/man1/*_%{name}-%{version}_apps_*
rm -f %{buildroot}%{_mandir}/man1/_home_rouault_dist_wrk_gdal_apps_.1*

%check
%if %{run_tests}
for i in -I/usr/lib/jvm/java/include{,/linux}; do
    java_inc="$java_inc $i"
done


pushd %{name}autotest-%{testversion}
  # Export test enviroment
  export PYTHONPATH=$PYTHONPATH:%{buildroot}%{python_sitearch}
  #TODO: Nötig?
  export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:%{buildroot}%{_libdir}
#  export LD_LIBRARY_PATH=$LD_LIBRARY_PATH:%%{buildroot}%%{_libdir}:$java_inc

  export GDAL_DATA=%{buildroot}%{_datadir}/%{name}/

  # Enable these tests on demand
  #export GDAL_RUN_SLOW_TESTS=1
  #export GDAL_DOWNLOAD_TEST_DATA=1

  # Remove some test cases that would require special preparation
  rm -rf ogr/ogr_pg.py        # No database available
  rm -rf ogr/ogr_mysql.py     # No database available
  rm -rf osr/osr_esri.py      # ESRI datum absent
  rm -rf osr/osr_erm.py       # File from ECW absent

  # Run tests but force normal exit in the end
  ./run_all.py || true
popd
%endif #%%{run_tests}


%post libs -p /sbin/ldconfig

%postun libs -p /sbin/ldconfig


%files
%{_bindir}/gdallocationinfo
%{_bindir}/gdal_contour
%{_bindir}/gdal_rasterize
%{_bindir}/gdal_translate
%{_bindir}/gdaladdo
%{_bindir}/gdalinfo
%{_bindir}/gdaldem
%{_bindir}/gdalbuildvrt
%{_bindir}/gdaltindex
%{_bindir}/gdalwarp
%{_bindir}/gdal_grid
%{_bindir}/gdalenhance
%{_bindir}/gdalmanage
%{_bindir}/gdalserver
%{_bindir}/gdalsrsinfo
%{_bindir}/gdaltransform
%{_bindir}/nearblack
%{_bindir}/ogr*
%{_bindir}/testepsg
%{_bindir}/gnmanalyse
%{_bindir}/gnmmanage
%{_bindir}/epsg_tr.py
%{_bindir}/esri2wkt.py
%{_bindir}/gcps2vec.py
%{_bindir}/gcps2wld.py
%{_bindir}/gdal2tiles.py
%{_bindir}/gdal2xyz.py
%{_bindir}/gdal_auth.py
%{_bindir}/gdal_calc.py
%{_bindir}/gdal_edit.py
%{_bindir}/gdal_fillnodata.py
%{_bindir}/gdal_merge.py
%{_bindir}/gdal_pansharpen.py
%{_bindir}/gdal_polygonize.py
%{_bindir}/gdal_proximity.py
%{_bindir}/gdal_retile.py
%{_bindir}/gdal_sieve.py
%{_bindir}/gdalchksum.py
%{_bindir}/gdalcompare.py
%{_bindir}/gdalident.py
%{_bindir}/gdalimport.py
%{_bindir}/gdalmove.py
%{_bindir}/mkgraticule.py
%{_bindir}/pct2rgb.py
%{_bindir}/rgb2pct.py
%{_mandir}/man1/gdal*.1*
%exclude %{_libdir}/libgdal.so.*
#%exclude /usr/lib64/libgdal.so.20.4.3
%exclude %{_mandir}/man1/gdal-config.1*
%exclude %{_mandir}/man1/gdal2tiles.1*
%exclude %{_mandir}/man1/gdal_fillnodata.1*
%exclude %{_mandir}/man1/gdal_merge.1*
%exclude %{_mandir}/man1/gdal_retile.1*
%exclude %{_mandir}/man1/gdal_sieve.1*
%{_mandir}/man1/nearblack.1*
%{_mandir}/man1/ogr*.1*
%{_mandir}/man1/gnm*.1.*
%exclude /usr/src/debug
%exclude /usr/share/gdal
%exclude %{_bindir}/%{name}-config
%exclude %{_bindir}/%{name}-config-%{cpuarch}
/usr%{bashcompletiondir}/gdal-bash-completion.sh

%files libs
%doc LICENSE.TXT NEWS
%{_libdir}/libgdal.so.*
%{_libdir}/python2.7/site-packages/GDAL-3.0.2-py2.7.egg-info/PKG-INFO
%{_libdir}/python2.7/site-packages/GDAL-3.0.2-py2.7.egg-info/SOURCES.txt
%{_libdir}/python2.7/site-packages/GDAL-3.0.2-py2.7.egg-info/dependency_links.txt
%{_libdir}/python2.7/site-packages/GDAL-3.0.2-py2.7.egg-info/not-zip-safe
%{_libdir}/python2.7/site-packages/GDAL-3.0.2-py2.7.egg-info/top_level.txt
%{_libdir}/python2.7/site-packages/gdal*
%{_libdir}/python2.7/site-packages/osr*
%{_libdir}/python2.7/site-packages/ogr*
%{_libdir}/python2.7/site-packages/osgeo/*
%{_datadir}/%{name}
#TODO: Possibly remove files like .dxf, .dgn, ...
%dir %{_libdir}/%{name}plugins

%files devel
%{_bindir}/%{name}-config
%{_bindir}/%{name}-config-%{cpuarch}
%{_mandir}/man1/gdal-config.1*
%dir %{_includedir}/%{name}
%{_includedir}/%{name}/*.h
%{_libdir}/*.so
%{_libdir}/pkgconfig/%{name}.pc

#%files doc
#%doc gdal_frmts ogrsf_frmts refman
#%doc refman

%changelog
* Sat May 26 2018 Christian Dersch <lupinix@mailbox.org> - 2.2.4-3
- rebuilt for cfitsio 3.450

* Tue Mar 27 2018 Björn Esser <besser82@fedoraproject.org> - 2.2.4-2
- Rebuilt for libjson-c.so.4 (json-c v0.13.1) on fc28

* Mon Mar 26 2018 Volker Fröhlich <volker27@gmx.at> - 2.2.4-1
- New upstream release

* Fri Mar 23 2018 Adam Williamson <awilliam@redhat.com> - 2.2.3-14
- Rebuild for poppler 0.63.0

* Tue Mar 06 2018 Björn Esser <besser82@fedoraproject.org> - 2.2.3-13
- Rebuilt for libjson-c.so.4 (json-c v0.13.1)

* Fri Feb 23 2018 Christian Dersch <lupinix@mailbox.org> - 2.2.3-12
- rebuilt for cfitsio 3.420 (so version bump)

* Wed Feb 14 2018 David Tardon <dtardon@redhat.com> - 2.2.3-11
- rebuild for poppler 0.62.0

* Wed Feb 14 2018 Volker Fröhlich <volker27@gmx.at> - 2.2.3-10
- Don't own /etc/bash_completion.d (BZ#1545012)

* Tue Feb 13 2018 Pavel Raiskup <praiskup@redhat.com> - 2.2.3-9
- silence some rpmlint warnings

* Tue Feb 13 2018 Tom Hughes <tom@compton.nu> - 2.2.3-8
- Add patch for bug by node-gdal tests and fixed upstream

* Tue Feb 13 2018 Tom Hughes <tom@compton.nu> - 2.2.3-7
- Use libtirpc for RPC routines

* Wed Feb 07 2018 Fedora Release Engineering <releng@fedoraproject.org> - 2.2.3-7
- Rebuilt for https://fedoraproject.org/wiki/Fedora_28_Mass_Rebuild

* Sat Jan 27 2018 Than Ngo <than@redhat.com> - - 2.2.3-6
- cleanup condition

* Thu Dec 14 2017 Merlin Mathesius <mmathesi@redhat.com> - 2.2.3-5
- Cleanup spec file conditionals

* Thu Dec 14 2017 Pavel Raiskup <praiskup@redhat.com> - 2.2.3-4
- drop bootstrap mode
- build-require mariadb-connector-c-devel (rhbz#1494096)

* Mon Dec 11 2017 Björn Esser <besser82@fedoraproject.org> - 2.2.3-3.1.bootstrap
- Add patch to cleanly build against json-c v0.13

* Sun Dec 10 2017 Björn Esser <besser82@fedoraproject.org> - 2.2.3-2.1.bootstrap
- Rebuilt for libjson-c.so.3

* Mon Dec 04 2017 Volker Froehlich <volker27@gmx.at> - 2.2.3-1
- New upstream release

* Wed Nov 29 2017 Volker Froehlich <volker27@gmx.at> - 2.2.2-2
- Re-enable bsb format (BZ#1432330)

* Fri Sep 22 2017 Volker Froehlich <volker27@gmx.at> - 2.2.2-1
- New upstream release
- Add new entries to the files sections

* Sun Sep 17 2017 Rex Dieter <rdieter@fedoraproject.org> - 2.1.4-11
- rebuild (armadillo)

* Mon Sep 11 2017 Rex Dieter <rdieter@fedoraproject.org> - 2.1.4-10
- support %%bootstrap mode, enable for rawhide (#1490492)
- segment POPPLER_OPTS, makes buildable on f25

* Fri Sep 08 2017 David Tardon <dtardon@redhat.com> - 2.1.4-9
- rebuild for poppler 0.59.0

* Sun Aug 20 2017 Zbigniew Jędrzejewski-Szmek <zbyszek@in.waw.pl> - 2.1.4-8
- Add Provides for the old name without %%_isa

* Sat Aug 19 2017 Orion Poplawski <orion@cora.nwra.com> - 2.1.4-7
- Handle new g2clib name in Fedora 27+

* Sat Aug 19 2017 Zbigniew Jędrzejewski-Szmek <zbyszek@in.waw.pl> - 2.1.4-6
- Python 2 binary package renamed to python2-gdal
  See https://fedoraproject.org/wiki/FinalizingFedoraSwitchtoPython3

* Thu Aug 03 2017 David Tardon <dtardon@redhat.com> - 2.1.4-5
- rebuild for poppler 0.57.0

* Wed Aug 02 2017 Fedora Release Engineering <releng@fedoraproject.org> - 2.1.4-4
- Rebuilt for https://fedoraproject.org/wiki/Fedora_27_Binutils_Mass_Rebuild

* Wed Jul 26 2017 Fedora Release Engineering <releng@fedoraproject.org> - 2.1.4-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_27_Mass_Rebuild

* Wed Jul 19 2017 Adam Williamson <awilliam@redhat.com> - 2.1.4-2
- Rebuild against MariaDB 10.2
- BuildRequires: javapackages-local, for a macro that got moved there

* Sat Jul 01 2017 Volker Froehlich <volker27@gmx.at> - 2.1.4-1
- New upstream release

* Sun Jun 04 2017 Jitka Plesnikova <jplesnik@redhat.com> - 2.1.3-4
- Perl 5.26 rebuild

* Tue Mar 28 2017 David Tardon <dtardon@redhat.com> - 2.1.3-3
- rebuild for poppler 0.53.0

* Wed Feb 01 2017 Sandro Mani <manisandro@gmail.com> - 2.1.3-2
- Rebuild (libwebp)

* Fri Jan 27 2017 Volker Froehlich <volker27@gmx.at> - 2.1.3-1
- New upstream release
- Don't run tests by default (BZ #1260151)

* Tue Jan 24 2017 Devrim Gündüz <devrim@gunduz.org> - 2.1.2-6
- Rebuilt for proj 4.9.3
- Fix many rpmlint warnings/errors.
- Add a workaround for the pkg-config change in rawhide.

* Mon Dec 19 2016 Miro Hrončok <mhroncok@redhat.com> - 2.1.2-5
- Rebuild for Python 3.6

* Fri Dec 16 2016 David Tardon <dtardon@redhat.com> - 2.1.2-4
- rebuild for poppler 0.50.0

* Thu Dec 01 2016 Orion Poplawski <orion@cora.nwra.com> - 2.1.2-3
- Rebuild for jasper 2.0
- Add patch to fix build with jasper 2.0

* Wed Nov 23 2016 David Tardon <dtardon@redhat.com> - 2.1.2-2
- rebuild for poppler 0.49.0

* Sun Oct 30 2016 Volker Froehlich <volker27@gmx.at> - 2.1.2-1
- New upstream release

* Sat Oct 22 2016 Orion Poplawski <orion@cora.nwra.com> - 2.1.1-2
- Use system libjson-c

* Fri Oct 21 2016 Marek Kasik <mkasik@redhat.com> - 2.1.1-2
- Rebuild for poppler-0.48.0

* Fri Aug 12 2016 Orion Poplawski <orion@cora.nwra.com> - 2.1.1-1
- Update to 2.1.1
- Add patch to fix bash-completion installation and install it (bug #1337143)

* Tue Jul 19 2016 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 2.1.0-8
- https://fedoraproject.org/wiki/Changes/Automatic_Provides_for_Python_RPM_Packages

* Mon Jul 18 2016 Marek Kasik <mkasik@redhat.com> - 2.1.0-7
- Rebuild for poppler-0.45.0

* Tue May 17 2016 Jitka Plesnikova <jplesnik@redhat.com> - 2.1.0-6
- Perl 5.24 rebuild

* Mon May 09 2016 Volker Froehlich <volker27@gmx.at> - 2.1.0-5
- Add missing BR for libkml

* Fri May 06 2016 Sandro Mani <manisandro@gmail.com>- 2.1.0-4
- Enable libKML support
  Resolves: #1332008

* Tue May 03 2016 Adam Williamson <awilliam@redhat.com> - 2.1.0-3
- rebuild for updated poppler

* Tue May  3 2016 Marek Kasik <mkasik@redhat.com> - 2.1.0-2
- Rebuild for poppler-0.43.0

* Mon May 02 2016 Jozef Mlich <imlich@fit.vutbr.cz> - 2.1.0-1
- New upstream release

* Mon Apr 18 2016 Tom Hughes <tom@compton.nu> - 2.0.2-5
- Rebuild for libdap change Resoloves: #1328104

* Tue Feb 16 2016 Elliott Sales de Andrade <quantum.analyst@gmail.com> - 2.0.2-4
- Add Python 3 support

* Sun Feb 14 2016 Volker Froehlich <volker27@gmx.at> - 2.0.2-3
- Add patch for GDAL issue #6360

* Mon Feb 08 2016 Volker Froehlich <volker27@gmx.at> - 2.0.2-2
- Rebuild for armadillo 6

* Thu Feb 04 2016 Volker Froehlich <volker27@gmx.at> - 2.0.2-1
- New upstream release
- Fix geos support (BZ #1284714)

* Wed Feb 03 2016 Fedora Release Engineering <releng@fedoraproject.org> - 2.0.1-6
- Rebuilt for https://fedoraproject.org/wiki/Fedora_24_Mass_Rebuild

* Fri Jan 22 2016 Marek Kasik <mkasik@redhat.com> 2.0.1-5
- Rebuild for poppler-0.40.0

* Fri Jan 15 2016 Adam Jackson <ajax@redhat.com> 2.0.1-4
- Rebuild for libdap soname bump

* Mon Dec 28 2015 Igor Gnatenko <i.gnatenko.brain@gmail.com> - 2.0.1-3
- Rebuilt for libwebp soname bump

* Sun Oct 18 2015 Volker Froehlich <volker27@gmx.at> - 2.0.1-2
- Solve BZ #1271906 (Build iso8211 and s57 utilities)

* Thu Sep 24 2015 Volker Froehlich <volker27@gmx.at> - 2.0.1-1
- Updated for 2.0.1; Add Perl module manpage

* Wed Sep 23 2015 Orion Poplawski <orion@cora.nwra.com> - 2.0.0-5
- Rebuild for libdap 3.15.1

* Sun Sep 20 2015 Volker Froehlich <volker27@gmx.at> - 2.0.0-4
- Support openjpeg2

* Thu Aug 27 2015 Jonathan Wakely <jwakely@redhat.com> - 2.0.0-3
- Rebuilt for Boost 1.59

* Sun Aug 09 2015 Jonathan Wakely <jwakely@redhat.com> 2.0.0-2
- Patch to set _XOPEN_SOURCE correctly (bug #1249703)

* Sun Jul 26 2015 Volker Froehlich <volker27@gmx.at> - 2.0.0-1
- Disable charls support due to build issues
- Solve a string formatting and comment errors in the Perl swig template

* Wed Jul 22 2015 Marek Kasik <mkasik@redhat.com> - 1.11.2-12
- Rebuild (poppler-0.34.0)

* Fri Jul  3 2015 José Matos <jamatos@fedoraproject.org> - 1.11.2-11
- Rebuild for armadillo 5(.xxx.y)

* Wed Jun 17 2015 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.11.2-10
- Rebuilt for https://fedoraproject.org/wiki/Fedora_23_Mass_Rebuild

* Fri Jun 12 2015 Volker Fröhlich <volker27@gmx.at> - 1.11.2-9
- Rebuild for Perl's dropped module_compat_5.20.*

* Tue Jun 09 2015 Dan Horák <dan[at]danny.cz> - 1.11.2-8
- add upstream patch for poppler >= 31

* Sat Jun 06 2015 Jitka Plesnikova <jplesnik@redhat.com> - 1.11.2-7
- Perl 5.22 rebuild

* Thu May 21 2015 Devrim Gündüz <devrim@gunduz.org> - 1.11.2-6
- Fix proj soname in ogr/ogrct.cpp. Patch from Sandro Mani
  <manisandro @ gmail.com>  Fixes #1212215.

* Sun May 17 2015 Orion Poplawski <orion@cora.nwra.com> - 1.11.2-5
- Rebuild for hdf5 1.8.15

* Sat Apr 18 2015 Ralf Corsépius <corsepiu@fedoraproject.org> - 1.11.2-4
- Rebuild for gcc-5.0.1 ABI changes.

* Tue Mar 31 2015 Orion Poplawski <orion@cora.nwra.com> - 1.11.2-3
- Rebuild for g2clib fix

* Wed Mar 11 2015 Devrim Gündüz <devrim@gunduz.org> - 1.11.2-2
- Rebuilt for proj 4.9.1

* Tue Feb 17 2015 Volker Fröhlich <volker27@gmx.at> - 1.11.2-1
- New release
- Remove obsolete sqlite patch

* Fri Jan 23 2015 Marek Kasik <mkasik@redhat.com> - 1.11.1-6
- Rebuild (poppler-0.30.0)

* Wed Jan 07 2015 Orion Poplawski <orion@cora.nwra.com> - 1.11.1-5
- Rebuild for hdf5 1.8.4

* Sat Dec  6 2014 Volker Fröhlich <volker27@gmx.at> - 1.11.1-4
- Apply upstream changeset 27949 to prevent a crash when using sqlite 3.8.7

* Tue Dec  2 2014 Jerry James <loganjerry@gmail.com> - 1.11.1-3
- Don't try to install perllocal.pod (bz 1161231)

* Thu Nov 27 2014 Marek Kasik <mkasik@redhat.com> - 1.11.1-3
- Rebuild (poppler-0.28.1)

* Fri Nov 14 2014 Dan Horák <dan[at]danny.cz> - 1.11.1-2
- update gdal-config for ppc64le

* Thu Oct  2 2014 Volker Fröhlich <volker27@gmx.at> - 1.11.1-1
- New release
- Correct test suite source URL

* Thu Aug 28 2014 Jitka Plesnikova <jplesnik@redhat.com> - 1.11.0-9
- Perl 5.20 rebuild

* Mon Aug 25 2014 Devrim Gündüz <devrim@gunduz.org> - 1.11.0-7
- Rebuilt for libgeotiff

* Sat Aug 16 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.11.0-7
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_22_Mass_Rebuild

* Thu Aug 14 2014 Volker Fröhlich <volker27@gmx.at> - 1.11.0-6
- Add aarch64 to gdal-config script (BZ#1129295)

* Fri Jul 25 2014 Peter Robinson <pbrobinson@fedoraproject.org> 1.11.0-5
- rebuild (libspatialite)

* Mon Jul 14 2014 Orion Poplawski <orion@cora.nwra.com> - 1.11.0-4
- Rebuild for libgeotiff 1.4.0

* Fri Jul 11 2014 Orion Poplawski <orion@cora.nwra.com> - 1.11.0-3
- Rebuild for libdap 3.13.1

* Sat Jun 07 2014 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.11.0-2
- Rebuilt for https://fedoraproject.org/wiki/Fedora_21_Mass_Rebuild

* Fri Apr 25 2014 Volker Fröhlich <volker27@gmx.at> - 1.11.0-1
- New upstream release
- Remove libgcj as BR, as it no longer exists in F21
- Re-enable ogdi and spatialite where possible
- Adapt Python-BR to python2-devel
- Obsolete Ruby bindings, due to the suggestion of Even Rouault
- Preserve timestamp of Fedora README file
- Explicitly create HTML documentation with Doxygen
- Make test execution conditional
- Truncate changelog

* Thu Apr 24 2014 Vít Ondruch <vondruch@redhat.com> - 1.10.1-7
- Rebuilt for https://fedoraproject.org/wiki/Changes/Ruby_2.1

* Fri Mar 28 2014 Michael Simacek <msimacek@redhat.com> - 1.10.1-6
- Use Requires: java-headless rebuild (#1067528)

* Fri Jan 10 2014 Orion Poplawski <orion@cora.nwra.com> - 1.10.1-5
- Rebuild for armadillo soname bump

* Wed Jan 08 2014 Orion Poplawski <orion@cora.nwra.com> - 1.10.1-4
- Rebuild for cfitsio 3.360

* Thu Jan 02 2014 Orion Poplawski <orion@cora.nwra.com> - 1.10.1-3
- Rebuild for libwebp soname bump

* Sat Sep 21 2013 Orion Poplawski <orion@cora.nwra.com> - 1.10.1-2
- Rebuild to pick up atlas 3.10 changes

* Sun Sep  8 2013 Volker Fröhlich <volker27@gmx.at> - 1.10.1-1
- New upstream release

* Fri Aug 23 2013 Orion Poplawski <orion@cora.nwra.com> - 1.10.0-1
- Update to 1.10.0
- Enable PCRE support
- Drop man patch applied upstream
- Drop dods patch fixed upstream
- Add more tex BRs to handle changes in texlive packaging
- Fix man page install location

* Mon Aug 19 2013 Marek Kasik <mkasik@redhat.com> - 1.9.2-12
- Rebuild (poppler-0.24.0)

* Sat Aug 03 2013 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.9.2-11
- Rebuilt for https://fedoraproject.org/wiki/Fedora_20_Mass_Rebuild

* Wed Jul 17 2013 Petr Pisar <ppisar@redhat.com> - 1.9.2-10
- Perl 5.18 rebuild

* Thu Jul 11 2013 Orion Poplawski <orion@cora.nwra.com> - 1.9.2-9
- Rebuild for cfitsio 3.350

* Mon Jun 24 2013 Volker Fröhlich <volker27@gmx.at> - 1.9.2-8
- Rebuild for poppler 0.22.5

* Wed Jun 12 2013 Orion Poplawski <orion@cora.nwra.com> - 1.9.2-7
- Update Java/JNI for new guidelines, also fixes bug #908065

* Thu May 16 2013 Orion Poplawski <orion@cora.nwra.com> - 1.9.2-6
- Rebuild for hdf5 1.8.11

* Mon Apr 29 2013 Peter Robinson <pbrobinson@fedoraproject.org> - 1.9.2-5
- Rebuild for ARM libspatialite issue

* Tue Mar 26 2013 Volker Fröhlich <volker27@gmx.at> - 1.9.2-4
- Rebuild for cfitsio 3.340

* Sun Mar 24 2013 Peter Robinson <pbrobinson@fedoraproject.org> - 1.9.2-3
- rebuild (libcfitsio)

* Wed Mar 13 2013 Vít Ondruch <vondruch@redhat.com> - 1.9.2-2
- Rebuild for https://fedoraproject.org/wiki/Features/Ruby_2.0.0

* Sun Mar 10 2013 Orion Poplawski <orion@cora.nwra.com> - 1.9.2-1
- Update to 1.9.2
- Drop poppler and java-swig patches applied upstream

* Fri Jan 25 2013 Devrim GÜNDÜZ <devrim@gunduz.org> - 1.9.1-18
- Rebuild with geos 3.3.7.

* Mon Jan 21 2013 Volker Fröhlich <volker27@gmx.at> - 1.9.1-17
- Rebuild due to libpoppler 0.22

* Fri Jan 18 2013 Adam Tkac <atkac redhat com> - 1.9.1-16
- rebuild due to "jpeg8-ABI" feature drop

* Fri Dec 28 2012 Richard W.M. Jones <rjones@redhat.com> - 1.9.1-15
- Rebuild, see
  http://lists.fedoraproject.org/pipermail/devel/2012-December/175685.html

* Thu Dec 13 2012 Peter Robinson <pbrobinson@fedoraproject.org> - 1.9.1-14
- Tweak -fpic CFLAGS to fix FTBFS on ARM

* Mon Dec  3 2012 Orion Poplawski <orion@cora.nwra.com> - 1.9.1-13
- Rebuild for hdf5 1.8.10

* Sun Dec  2 2012 Bruno Wolff III <bruno@wolff.to> - 1.9.1-12
- Rebuild for libspatialite soname bump

* Thu Aug  9 2012 Volker Fröhlich <volker27@gmx.at> - 1.9.1-11
- Correct and extend conditionals for ppc andd ppc64, considering libspatialite
  Related to BZ #846301

* Sun Jul 29 2012 José Matos <jamatos@fedoraproject.org> - 1.9.1-10
- Use the correct shell idiom "if true" instead of "if 1"

* Sun Jul 29 2012 José Matos <jamatos@fedoraproject.org> - 1.9.1-9
- Ignore for the moment the test for armadillo (to be removed after gcc 4.7.2 release)

* Fri Jul 27 2012 José Matos <jamatos@fedoraproject.org> - 1.9.1-8
- Rebuild for new armadillo

* Fri Jul 20 2012 Peter Robinson <pbrobinson@fedoraproject.org> - 1.9.1-7
- Build with PIC

* Thu Jul 19 2012 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.9.1-6
- Rebuilt for https://fedoraproject.org/wiki/Fedora_18_Mass_Rebuild

* Tue Jul 10 2012 Petr Pisar <ppisar@redhat.com> - 1.9.1-5
- Perl 5.16 rebuild

* Sat Jul  7 2012 Volker Fröhlich <volker27@gmx.at> - 1.9.1-4
- Delete unnecessary manpage, that seems to be created with
  new Doxygen (1.8.1 or 1.8.1.1)

* Mon Jul  2 2012 Marek Kasik <mkasik@redhat.com> - 1.9.1-3
- Rebuild (poppler-0.20.1)

* Mon Jun 11 2012 Petr Pisar <ppisar@redhat.com> - 1.9.1-2
- Perl 5.16 rebuild

* Wed May 23 2012 Volker Fröhlich <volker27@gmx.at> - 1.9.1-1
- New upstream release
- Update poppler patch
- Add cleaner script

* Sun May 20 2012 Volker Fröhlich <volker27@gmx.at> - 1.9.0-5
- Patches for libpoppler 0.20, libdap 3.11.3 and swig 2.0.6

* Thu May 10 2012 Volker Fröhlich <volker27@gmx.at> - 1.9.0-4
- Correct provides-filtering as of https://fedoraproject.org/wiki/Packaging:AutoProvidesAndRequiresFiltering#Usage
- Support webp
- Remove bogus libjpeg-turbo conditional
- Update Ruby ABI version to 1.9.1
- Install Ruby bindings to vendorarchdir on F17 and later
- Conditionals for Ruby specific elements for versions prior F17 and for EPEL
- Correct quotes for CFLAGS and Ruby
- Disable ogdi, until BZ#816282 is resolved

* Wed Apr 25 2012 Orion Poplawski <orion@cora.nwra.com> - 1.9.0-2
- Rebuild for cfitsio 3.300

* Sun Feb 26 2012 Volker Fröhlich <volker27@gmx.at> - 1.9.0-1
- Completely re-work the original spec-file
  The major changes are:
- Add a libs sub-package
- Move Python scripts to python sub-package
- Install the documentation in a better way and with less slack
- jar's filename is versionless
- Update the version in the Maven pom automatically
- Add a plugins directory
- Add javadoc package and make the man sub-package noarch
- Support many additional formats
- Drop static sub-package as no other package uses it as BR
- Delete included libs before building
- Drop all patches, switch to a patch for the manpages, patch for JAVA path
- Harmonize the use of buildroot and RPM_BUILD_ROOT
- Introduce testversion macro

* Sun Feb 19 2012 Volker Fröhlich <volker27@gmx.at> - 1.7.3-14
- Require Ruby abi
- Add patch for Ruby 1.9 include dir, back-ported from GDAL 1.9
- Change version string for gdal-config from <version>-fedora to
  <version>
- Revert installation path for Ruby modules, as it proofed wrong
- Use libjpeg-turbo

* Thu Feb  9 2012 Volker Fröhlich <volker27@gmx.at> - 1.7.3-13
- Rebuild for Ruby 1.9
  http://lists.fedoraproject.org/pipermail/ruby-sig/2012-January/000805.html

* Tue Jan 10 2012 Volker Fröhlich <volker27@gmx.at> - 1.7.3-12
- Remove FC10 specific patch0
- Versioned MODULE_COMPAT_ Requires for Perl (BZ 768265)
- Add isa macro to base package Requires
- Remove conditional for xerces_c in EL6, as EL6 has xerces_c
  even for ppc64 via EPEL
- Remove EL4 conditionals
- Replace the python_lib macro definition and install Python bindings
  to sitearch directory, where they belong
- Use correct dap library names for linking
- Correct Ruby installation path in the Makefile instead of moving it later
- Use libdir variable in ppc64 Python path
- Delete obsolete chmod for Python libraries
- Move correction for Doxygen footer to prep section
- Delete bundled libraries before building
- Build without bsb and remove it from the tarball
- Use mavenpomdir macro and be a bit more precise on manpages in
  the files section
- Remove elements for grass support --> Will be replaced by plug-in
- Remove unnecessary defattr
- Correct version number in POM
- Allow for libpng 1.5

* Tue Dec 06 2011 Adam Jackson <ajax@redhat.com> - 1.7.3-11
- Rebuild for new libpng

* Tue May 17 2011 Orion Poplawski <orion@cora.nwra.com> - 1.7.3-10
- Rebuild for hdf5 1.8.7

* Fri Apr 22 2011 Volker Fröhlich <volker27@gmx.at> - 1.7.3-9
- Patched spaces problem for Mapinfo files (mif)
  (http://trac.osgeo.org/gdal/ticket/3694)
- Replaced all define macros with global
- Corrected ruby_sitelib to ruby_sitearch
- Use python_lib and ruby_sitearch instead of generating lists
- Added man-pages for binaries
- Replaced mkdir and install macros
- Removed Python files from main package files section, that
  effectively already belonged to the Python sub-package

* Mon Apr 11 2011 Volker Fröhlich <volker27@gmx.at> - 1.7.3-8
- Solved image path problem with Latex
- Removed with-tiff and updated with-sqlite to with-sqlite3
- Add more refman documents
- Adapted refman loop to actual directories
- Harmonized buildroot macro use

* Thu Mar 31 2011 Orion Poplawski <orion@cora.nwra.com> - 1.7.3-7
- Rebuild for netcdf 4.1.2

* Wed Mar 23 2011 Dan Horák <dan@danny.cz> - 1.7.3-6
- rebuilt for mysql 5.5.10 (soname bump in libmysqlclient)

* Sun Mar 20 2011 Volker Fröhlich <volker27@gmx.at> - 1.7.3-5
- Dropped unnecessary encoding conversion for Russian refman
- Install Russian refman
- Don't try to install refman for sdts and dgn, as they fail to compile
- Added -p to post and postun
- Remove private-shared-object-provides for Python and Perl
- Remove installdox scripts
- gcc 4.6 doesn't accept -Xcompiler

* Thu Mar 10 2011 Kalev Lember <kalev@smartlink.ee> - 1.7.3-4
- Rebuilt with xerces-c 3.1

* Tue Feb 08 2011 Fedora Release Engineering <rel-eng@lists.fedoraproject.org> - 1.7.3-3
- Rebuilt for https://fedoraproject.org/wiki/Fedora_15_Mass_Rebuild

* Sun Nov 21 2010 Viji Nair <viji [AT] fedoraproject DOT org> - 1.7.3-2
- Install all the generated pdf documentation.
- Build documentation as a separate package.
- Spec cleanup

* Fri Nov 19 2010 Viji Nair <viji [AT] fedoraproject DOT org> - 1.7.3-1
- Update to latest upstream version
- Added jnis
- Patches updated with proper version info
- Added suggestions from Ralph Apel <r.apel@r-apel.de>
        + Versionless symlink for gdal.jar
        + Maven2 pom
        + JPP-style depmap
        + Use -f XX.files for ruby and python
