#!/bin/sh

if [ $# -lt 2 ] ; then
  echo "Usage: mkbindist.sh [-dev] version platform [-install]"
  echo
  echo "Example: mkbindist.sh 1.1.5 linux"
  exit
fi

if [ $1 == "-dev" ] ; then
  STRIP_DEV=0
  shift
else
  STRIP_DEV=1
fi

VERSION=$1
PLATFORM=$2

#
# 	Process the version into a suitable format for overriding internal
#       information.
#
VERSION_NUM=`echo $VERSION | tr -c -d 0123456789`000
VERSION_NUM=${VERSION_NUM:0:4}
RELEASE_DATE=`date +%Y%m%d`

USER_DEFS="-DGDAL_VERSION_NUM=$VERSION_NUM -DGDAL_RELEASE_DATE=$RELEASE_DATE -DGDAL_RELEASE_NAME=\\\"$VERSION\\\""

#
#	Build and install.
#

DIST_DIR=gdal-${VERSION}-${PLATFORM}-bin
FULL_DIST_DIR=`pwd`/$DIST_DIR

rm -rf $DIST_DIR
mkdir $DIST_DIR

# ensure gdal_misc (with GDALVersionInfo()) is recompiled.

rm -f gcore/gdal_misc.o apps/gdal-config apps/gdal-config-inst
make 'prefix='$FULL_DIST_DIR 'INST_PYMOD='$FULL_DIST_DIR/pymod 'USER_DEFS='"$USER_DEFS" install
if test \! make ; then
  exit
fi									

#
#      Copy in other info of interest.
#

mkdir $DIST_DIR/html
cp html/* $DIST_DIR/html

cp dist_docs/README_UNX_BIN.TXT $DIST_DIR
cp dist_docs/install_unx.sh $DIST_DIR
cc -o $DIST_DIR/bin/burnpath dist_docs/burnpath.c

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

echo "Compressing ${DIST_DIR}.tar ... this may take a moment."
gzip -9 ${DIST_DIR}.tar

echo "Prepared: "${DIST_DIR}.tar.gz

TARGETDIR=remotesensing.org:/ftp/remotesensing/pub/gdal

if test "$3" = "-install" ; then
  scp ${DIST_DIR}.tar.gz $TARGETDIR
fi
