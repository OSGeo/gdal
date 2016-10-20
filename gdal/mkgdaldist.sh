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
  echo " -branch   - path to SVN branch, trunk is used if not provided"
  echo " -rc       - gives a release candidate id to embed in filenames"
  echo "Example: mkgdaldist.sh 1.8.0 -branch branches/1.8 -rc RC2"
  echo "or       mkgdaldist.sh 1.10.0beta2"
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
  forcebranch=$3
  shift
  shift
else
  forcebranch="trunk"
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
echo "* Downloading GDAL sources from SVN..."
rm -rf dist_wrk
mkdir dist_wrk
cd dist_wrk

SVNURL="http://svn.osgeo.org/gdal"
SVNBRANCH=${forcebranch}
SVNMODULE="gdal"

echo "Generating package '${GDAL_VERSION}' from '${SVNBRANCH}' branch"
echo

# Disable for now, seems to depend on modern SVN versions.
#SVN_CONFIG="--config-option config:miscellany:use-commit-times=yes"

svn checkout -q ${SVNURL}/${SVNBRANCH}/${SVNMODULE} ${SVNMODULE} ${SVN_CONFIG}

if [ \! -d gdal ] ; then
	echo "svn checkout reported an error ... abandoning mkgdaldist"
	cd ..
	rm -rf dist_wrk
	exit
fi

#
# Make some updates and cleaning
#
echo "* Updating release date..."
if test "$forcedate" != "no" ; then
  echo "Forcing Date To: $forcedate"
  rm -f gdal/gcore/gdal_new.h
  sed -e "/define GDAL_RELEASE_DATE/s/20[0-9][0-9][0-9][0-9][0-9][0-9]/$forcedate/" gdal/gcore/gdal.h > gdal/gcore/gdal_new.h
  mv gdal/gcore/gdal_new.h gdal/gcore/gdal.h
fi

echo "* Cleaning .svn directories under $PWD..."
find gdal -name .svn | xargs rm -rf
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

if test -f "doxygen_sqlite3.db"; then
    rm -f doxygen_sqlite3.db
fi

cd ${CWD}

#
# Generate SWIG interface for C#
#
echo "* Generating SWIG C# interfaces..."
CWD=${PWD}
cd gdal/swig/csharp
./mkinterface.sh
cd ${CWD}

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

rm -f ../gdal-${GDAL_VERSION}${RC}.tar.gz ../gdal${COMPRESSED_VERSION}${RC}.zip

tar cf ../gdal-${GDAL_VERSION}${RC}.tar gdal-${GDAL_VERSION}
xz -k9e ../gdal-${GDAL_VERSION}${RC}.tar
gzip -9 ../gdal-${GDAL_VERSION}${RC}.tar
zip -qr ../gdal${COMPRESSED_VERSION}${RC}.zip gdal-${GDAL_VERSION}

echo "* Generating MD5 sums ..."

OSTYPE=`uname -s`
if test "$OSTYPE" = "Darwin" ; then
MD5=md5
else
MD5=md5sum
fi

cd ..
$MD5 gdal-${GDAL_VERSION}${RC}.tar.xz > gdal-${GDAL_VERSION}${RC}.tar.xz.md5
$MD5 gdal-${GDAL_VERSION}${RC}.tar.gz > gdal-${GDAL_VERSION}${RC}.tar.gz.md5
$MD5 gdal${COMPRESSED_VERSION}${RC}.zip > gdal${COMPRESSED_VERSION}${RC}.zip.md5

echo "* Cleaning..."
rm -rf dist_wrk

echo "*** The End ***"
