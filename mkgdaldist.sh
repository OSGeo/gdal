#!/bin/sh

if [ $# -ne 1 ] ; then
  echo "Usage: mkgdaldist version"
  echo
  echo "Example: mkgdaldist 1.1.1"
  exit
fi

GDAL_VERSION=$1
COMPRESSED_VERSION=`echo $GDAL_VERSION | tr -d .`

rm -rf dist_wrk  
mkdir dist_wrk
cd dist_wrk

export CVSROOT=:pserver:anonymous@cvs.remotesensing.org:/cvsroot

echo "Please type anonymous if prompted for a password."
cvs login

cvs checkout gdal

if [ \! -d gdal ] ; then
  echo "cvs checkout reported an error ... abandoning mkgdaldist"
  exit
fi

find gdal -name CVS -exec rm -rf {} \;

rm -rf gdal/viewer

mv gdal gdal-${GDAL_VERSION}

tar cf ../gdal-${GDAL_VERSION}.tar.gz gdal-${GDAL_VERSION}
gzip -9 ../gdal-${GDAL_VERSION}.tar.gz
zip -r ../gdal${COMPRESSED_VERSION}.zip gdal-${GDAL_VERSION}

