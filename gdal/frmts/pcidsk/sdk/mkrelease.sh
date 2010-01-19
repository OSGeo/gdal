#!/bin/sh

if [ $# -lt 1 ] ; then
  echo "Usage: mkrelease.sh <version>"
  echo " <version> - version number used in name of generated archive."
  echo "Example: mkrelease.sh 0.8"
  exit
fi

#
# Processing script input arguments
#
VERSION=$1

#
# Checkout sources from the repository
#
echo "* Downloading sources from SVN..."
rm -rf dist_wrk  
mkdir dist_wrk
cd dist_wrk

svn export http://svn.osgeo.org/gdal/sandbox/warmerdam/pcidsk pcidsk

if [ \! -d pcidsk ] ; then
	echo "svn checkout reported an error ... abandoning mkrelease"
	cd ..
	rm -rf dist_wrk
	exit
fi

#
# Make distribution packages
#
echo "* Making distribution packages..."

mv pcidsk pcidsk-${VERSION}

rm -f ../gdal-${VERSION}.{tar.gz,zip}

tar cf ../pcidsk-${VERSION}.tar pcidsk-${VERSION}
gzip -9 ../pcidsk-${VERSION}.tar
zip -r ../pcidsk-${VERSION}.zip pcidsk-${VERSION}

echo "* Cleaning..."
cd ..
rm -rf dist_wrk

echo "*** The End ***"
