#!/bin/sh

if [ $# -lt 2 ] ; then
  echo "Usage: mkbindist.sh [-nodev] version platform [-install]"
  echo
  echo "Example: mkbindist.sh 1.1.5 linux"
  exit
fi

if [ $1 == "-nodev" ] ; then
  STRIP_DEV=1
  shift
else
  STRIP_DEV=0
fi

VERSION=$1
PLATFORM=$2

#
#	Build and install.
#

DIST_DIR=gdal-${PLATFORM}-bin.${VERSION}
FULL_DIST_DIR=`pwd`/$DIST_DIR

rm -rf $DIST_DIR
mkdir $DIST_DIR


make 'prefix='$FULL_DIST_DIR 'INST_PYMOD='$FULL_DIST_DIR/pymod install
if test \! make ; then
  exit
fi									

#
#      Copy in other info of interest.
#

mkdir $DIST_DIR/html
cp html/* $DIST_DIR/html

#
# Clean anything we don't want for non-developer releases.
#
if [ "$STRIP_DEV" == "1" ] ; then
  rm -f $DIST_DIR/html/class_*
  rm -f $DIST_DIR/html/struct_*
  rm -f $DIST_DIR/html/*-source.html
  rm -f $DIST_DIR/lib/*.a
  rm -rf $DIST_DIR/include
fi

#
# Pack up
#

rm -f ${DIST_DIR}.tar.gz
tar cf ${DIST_DIR}.tar ${DIST_DIR}
gzip -9 ${DIST_DIR}.tar

echo "Prepared: "${DIST_DIR}.tar.gz

TARGETDIR=remotesensing.org:/ftp/remotesensing/pub/geotiff/gdal

if test "$3" = "-install" ; then
  scp ${DIST_DIR}.tar.gz $TARGETDIR
fi
