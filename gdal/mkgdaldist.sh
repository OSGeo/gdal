#!/bin/sh
#
# $Id$
#
# mkgdaldist.sh - prepares GDAL source distribution package
#

# Doxgen 1.7.1 has a bug related to man pages. See https://trac.osgeo.org/gdal/ticket/6048
echo $(doxygen --version) | xargs python -c "import sys; v = sys.argv[1].split('.'); v=int(v[0])*10000+int(v[1])*100+int(v[2]); sys.exit(v < 10704)"
rc=$?
if test $rc != 0; then
    echo "Wrong Doxygen version. 1.7.4 or later required"
    exit $rc;
fi

if [ $# -lt 1 ] ; then
  echo "Usage: mkgdaldist.sh <version> [-date date] [-branch branch] [-rc n]"
  echo " <version> - version number used in name of generated archive."
  echo " -date     - date of package generation, current date used if not provided"
  echo " -branch   - path to git branch or tag, master is used if not provided"
  echo " -rc       - gives a release candidate id to embed in filenames"
  echo "Example: mkgdaldist.sh 2.2.4 -branch v2.2.4 -rc RC1"
  echo "or       mkgdaldist.sh 2.3.0beta1"
  exit
fi

#
# Processing script input arguments
#
GDAL_VERSION=$1
COMPRESSED_VERSION=`echo $GDAL_VERSION | tr -d .`

if test "$2" = "-date" ; then
  forcedate=$3
  shift
  shift
else
  forcedate=no
fi

if test "$2" = "-branch"; then
  BRANCH=$3
  shift
  shift
else
  BRANCH="master"
fi

if test "$2" = "-rc"; then
  RC=$3
  shift
  shift
else
  RC=""
fi

#
# Checkout GDAL sources from the repository
#
echo "* Downloading GDAL sources from git..."
rm -rf dist_wrk
mkdir dist_wrk
cd dist_wrk

GITURL="https://github.com/OSGeo/gdal"

echo "Generating package '${GDAL_VERSION}' from '${BRANCH}' branch"
echo

git clone --depth 1 -b ${BRANCH} ${GITURL}
cd gdal

if [ \! -d gdal ] ; then
	echo "svn checkout reported an error ... abandoning mkgdaldist"
	cd ..
	rm -rf dist_wrk
	exit
fi

#
# Make some updates and cleaning
#
if test "$forcedate" != "no" ; then
  echo "* Updating release date..."
  echo "Forcing Date To: $forcedate"
  rm -f gdal/gcore/gdal_new.h
  sed -e "/define GDAL_RELEASE_DATE/s/20[0-9][0-9][0-9][0-9][0-9][0-9]/$forcedate/" gdal/gcore/gdal.h > gdal/gcore/gdal_new.h
  mv gdal/gcore/gdal_new.h gdal/gcore/gdal.h
fi

echo "* Cleaning .gitignore under $PWD..."
rm -f gdal/.gitignore

#
# Generate man pages
#
echo "* Generating man pages..."
CWD=${PWD}
cd gdal
if test -d "man"; then
    rm -rf man
fi

(cat Doxyfile ; echo "ENABLED_SECTIONS=man"; echo "INPUT=apps swig/python/scripts"; echo "FILE_PATTERNS=*.cpp *.dox"; echo "GENERATE_HTML=NO"; echo "GENERATE_MAN=YES"; echo "QUIET=YES") | doxygen -

if test ! -d "man"; then
    echo " make man failed"
fi

rm -f doxygen_sqlite3.db
rm -f man/man1/*_dist_wrk_gdal_gdal_apps_.1

cd ${CWD}

# They currently require SWIG 1.3.X, which is not convenient as we need
# newer SWIG for newer Python versions
echo "SWIG C# interfaces *NOT* generated !"

#
# Generate SWIG interface for C#
#
#echo "* Generating SWIG C# interfaces..."
#CWD=${PWD}
#cd gdal/swig/csharp
#./mkinterface.sh
#cd ${CWD}

#
# Generate SWIG interface for Perl
#
echo "* Generating SWIG Perl interfaces..."
CWD=${PWD}
cd gdal/swig/perl

rm *wrap*
touch ../../GDALmake.opt
make generate
rm -f ../../GDALmake.opt
cd ${CWD}

#
# Make distribution packages
#
echo "* Making distribution packages..."
rm -f gdal/VERSION
echo $GDAL_VERSION > gdal/VERSION

mv gdal gdal-${GDAL_VERSION}

rm -f ../../gdal-${GDAL_VERSION}${RC}.tar.gz ../../gdal${COMPRESSED_VERSION}${RC}.zip

tar cf ../../gdal-${GDAL_VERSION}${RC}.tar gdal-${GDAL_VERSION}
xz -k9e ../../gdal-${GDAL_VERSION}${RC}.tar
gzip -9 ../../gdal-${GDAL_VERSION}${RC}.tar
zip -qr ../../gdal${COMPRESSED_VERSION}${RC}.zip gdal-${GDAL_VERSION}

mv autotest gdalautotest-${GDAL_VERSION}
rm ../../gdalautotest-${GDAL_VERSION}${RC}.tar.gz
rm ../../gdalautotest-${GDAL_VERSION}${RC}.zip
tar cf ../../gdalautotest-${GDAL_VERSION}${RC}.tar.gz gdalautotest-${GDAL_VERSION}
zip -qr ../../gdalautotest-${GDAL_VERSION}${RC}.zip gdalautotest-${GDAL_VERSION}

cd gdal-${GDAL_VERSION}
echo "GDAL_VER=${GDAL_VERSION}" > GDALmake.opt
cd frmts/grass
make dist
mv *.tar.gz ../../../../..
cd ../../..

echo "* Generating MD5 sums ..."

OSTYPE=`uname -s`
if test "$OSTYPE" = "Darwin" ; then
MD5=md5
else
MD5=md5sum
fi

cd ../..
$MD5 gdal-${GDAL_VERSION}${RC}.tar.xz > gdal-${GDAL_VERSION}${RC}.tar.xz.md5
$MD5 gdal-${GDAL_VERSION}${RC}.tar.gz > gdal-${GDAL_VERSION}${RC}.tar.gz.md5
$MD5 gdal${COMPRESSED_VERSION}${RC}.zip > gdal${COMPRESSED_VERSION}${RC}.zip.md5

echo "* Cleaning..."
rm -rf dist_wrk

echo "*** The End ***"
