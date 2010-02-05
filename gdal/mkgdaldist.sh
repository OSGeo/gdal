#!/bin/sh
#
# $Id$
#
# mkgdaldist.sh - prepares GDAL source distribution package
#
if [ $# -lt 1 ] ; then
  echo "Usage: mkgdaldist.sh <version> [-date date] [-branch branch]"
  echo " <version> - version number used in name of generated archive."
  echo " -date     - date of package generation, current date used if not provided"
  echo " -branch   - path to SVN branch, trunk is used if not provided"
  echo "Example: mkgdaldist.sh 1.1.4"
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
else
  forcebranch="trunk"
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
 
svn checkout ${SVNURL}/${SVNBRANCH}/${SVNMODULE} ${SVNMODULE}

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

#
# Generate man pages
#
echo "* Generating man pages..."
CWD=${PWD}
cd gdal
if test -d "man"; then
    rm -rf man
fi

(cat Doxyfile ; echo "ENABLED_SECTIONS=man"; echo "INPUT=doc ogr"; echo "FILE_PATTERNS=*utilities.dox"; echo "GENERATE_HTML=NO"; echo "GENERATE_MAN=YES") | doxygen -

if test ! -d "man"; then
    echo " make man failed"
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
make generate
cd ${CWD}

#
# Make distribution packages
#
echo "* Making distribution packages..."
rm -rf gdal/viewer
rm -rf gdal/dist_docs

rm -f gdal/VERSION
echo $GDAL_VERSION > gdal/VERSION

mv gdal gdal-${GDAL_VERSION}

rm -f ../gdal-${GDAL_VERSION}.tar.gz ../gdal${COMPRESSED_VERSION}.zip

tar cf ../gdal-${GDAL_VERSION}.tar gdal-${GDAL_VERSION}
gzip -9 ../gdal-${GDAL_VERSION}.tar
zip -r ../gdal${COMPRESSED_VERSION}.zip gdal-${GDAL_VERSION}

echo "* Generating MD5 sums ..."
md5 ../gdal-${GDAL_VERSION}.tar.gz > ../gdal-${GDAL_VERSION}.tar.gz.md5
md5 ../gdal${COMPRESSED_VERSION}.zip > ../gdal${COMPRESSED_VERSION}.zip.md5
echo "* Cleaning..."
cd ..
rm -rf dist_wrk

echo "*** The End ***"
