#!/bin/sh

if [ $# -lt 2 ] ; then
  echo "Usage: mkbindist.sh version platform [-install]"
  echo
  echo "Example: mkbindist.sh 1.1.5 linux"
  exit
fi

VERSION=$1
PLATFORM=$2

# Ensure all required components are built.

if test \! make ; then
  exit
fi

#
#	Prepare tree.
#

DIST_DIR=libgeotiff-${PLATFORM}-bin.${VERSION}

rm -rf $DIST_DIR
mkdir $DIST_DIR

mkdir $DIST_DIR/bin
cp bin/geotifcp bin/listgeo $DIST_DIR/bin

# The file list is copied from Makefile.in:GT_INCLUDE_FILES

mkdir $DIST_DIR/include
cp xtiffio.h xtiffiop.h geotiff.h geotiffio.h geovalues.h \
    geonames.h geokeys.h geo_tiffp.h geo_config.h geo_keyp.h \
    geo_normalize.h cpl_serv.h cpl_csv.h \
    epsg_datum.inc epsg_gcs.inc epsg_pm.inc epsg_units.inc geo_ctrans.inc \
    epsg_ellipse.inc epsg_pcs.inc epsg_proj.inc epsg_vertcs.inc geokeys.inc \
    $DIST_DIR/include

mkdir $DIST_DIR/lib
cp libgeotiff.a $DIST_DIR/lib
if test -f libgeotiff.so.$VERSION ; then
  cp libgeotiff.so.$VERSION $DIST_DIR/lib
  (cd $DIST_DIR/lib ; ln -s libgeotiff.so.$VERSION libgeotiff.so)
fi

mkdir $DIST_DIR/share
mkdir $DIST_DIR/share/epsg_csv
cp csv/*.csv $DIST_DIR/share/epsg_csv

cp README_BIN $DIST_DIR/README

rm -f ${DIST_DIR}.tar.gz
tar cf ${DIST_DIR}.tar ${DIST_DIR}
gzip -9 ${DIST_DIR}.tar

echo "Prepared: "${DIST_DIR}.tar.gz

TARGETDIR=remotesensing.org:/ftp/remotesensing/pub/geotiff/libgeotiff

if test "$3" = "-install" ; then
  scp ${DIST_DIR}.tar.gz $TARGETDIR
fi
