#!/bin/sh

if [ $# -lt 1 ] ; then
  echo "Usage: mkgdaldist version [-install]"
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

rm -f ../gdal-${GDAL_VERSION}.tar.gz ../gdal${COMPRESSED_VERSION}.zip

tar cf ../gdal-${GDAL_VERSION}.tar.gz gdal-${GDAL_VERSION}
gzip -9 ../gdal-${GDAL_VERSION}.tar.gz
zip -r ../gdal${COMPRESSED_VERSION}.zip gdal-${GDAL_VERSION}

cd ..
rm -rf dist_wrk

TARGETDIR=/ftp/remotesensing/pub/gdal
if test "$2" = "-install" ; then
  if test \! -d $TARGETDIR ; then
    echo "Can't find $TARGETDIR ... -install failed."
    exit
  fi

  echo "Installing: " $TARGETDIR/gdal-${GDAL_VERSION}.tar.gz
  rm -f $TARGETDIR/gdal-${GDAL_VERSION}.tar.gz
  cp gdal-${GDAL_VERSION}.tar.gz $TARGETDIR

  echo "Installing: " $TARGETDIR/gdal${COMPRESSED_VERSION}.zip
  rm -f $TARGETDIR/gdal${COMPRESSED_VERSION}.zip
  cp gdal${COMPRESSED_VERSION}.zip $TARGETDIR
fi
