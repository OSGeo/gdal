#!/bin/sh

if [ $# -lt 1 ] ; then
  echo "Usage: mk_win_bin_dist.sh version [-install]"
  echo
  echo "Example: mk_win_bin_dist.sh 114"
  exit
fi

TARGETDIR=remotesensing.org:/ftp/remotesensing/pub/gdal
PROJ_DLL=/d/bin/proj.dll

SHORT_VERSION=11
VERSION=$1
DIST_DIR=gdal$VERSION
DIST_FILE=gdal-${VERSION}-ntbin.zip

rm -rf $DIST_DIR
mkdir $DIST_DIR

mkdir $DIST_DIR/bin
mkdir $DIST_DIR/data
mkdir $DIST_DIR/include
mkdir $DIST_DIR/lib
mkdir $DIST_DIR/html
mkdir $DIST_DIR/pymod

cp gdal$SHORT_VERSION.dll $DIST_DIR/bin
cp apps/*.exe ogr/ogrinfo.exe ogr/ogr2ogr.exe $DIST_DIR/bin
cp $PROJ_DLL $DIST_DIR/bin
cp gdal_i.lib gdal.lib $DIST_DIR/lib
cp data/*.csv $DIST_DIR/data
cp port/*.h gcore/*.h ogr/*.h ogr/ogrsf_frmts/*.h $DIST_DIR/include
cp html/*.* $DIST_DIR/html
cp pymod/*.dll pymod/*.py $DIST_DIR/pymod
cp dist_docs/README_WIN_BIN.TXT $DIST_DIR/README.TXT
cp dist_docs/SETUP_GDAL.BAT $DIST_DIR

rm -f $DIST_FILE
zip -r $DIST_FILE $DIST_DIR

echo $DIST_FILE ready.

if test "$2" = "-install" ; then
  scp $DIST_FILE $TARGETDIR
fi
