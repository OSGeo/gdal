#!/bin/sh

if [ $# -lt 1 ] ; then
  echo "Usage: mkgdaldist version [-nologin]"
  echo
  echo "Example: mkgdaldist 1.1.4"
  exit
fi

GDAL_VERSION=$1

if test "$GDAL_VERSION" != "`cat VERSION`" ; then
  echo
  echo "NOTE: local VERSION file (`cat VERSION`) does not match supplied version ($GDAL_VERSION)."
  echo "      Consider updating local VERSION file, and committing to CVS." 
  echo
fi

rm -rf dist_wrk  
mkdir dist_wrk
cd dist_wrk

export CVSROOT=:pserver:cvsanon@cvs.maptools.org:/cvs/maptools/cvsroot

if test "$2" = "-nologin" -o "$3" = "-nologin" ; then
  echo "Skipping login"
else
  echo "Please type anonymous if prompted for a password."
  cvs login
fi

cvs -Q checkout gdalautotest

if [ \! -d gdalautotest ] ; then
  echo "cvs checkout reported an error ... abandoning mktestdist"
  cd ..
  rm -rf dist_wrk
  exit
fi

find gdalautotest -name CVS -exec rm -rf {} \;

mv gdalautotest gdalautotest-${GDAL_VERSION}

rm -f ../gdalautotest-${GDAL_VERSION}.tar.gz

tar cf ../gdalautotest-${GDAL_VERSION}.tar gdalautotest-${GDAL_VERSION}
gzip -9 ../gdalautotest-${GDAL_VERSION}.tar

cd ..
rm -rf dist_wrk
