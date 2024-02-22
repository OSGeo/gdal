#!/bin/sh

set -e

SCRIPT_DIR=$(dirname "$0")
case $SCRIPT_DIR in
    "/"*)
        ;;
    ".")
        SCRIPT_DIR=$(pwd)
        ;;
    *)
        SCRIPT_DIR=$(pwd)/$(dirname "$0")
        ;;
esac
$SCRIPT_DIR/../common_install.sh

GDAL_TOPDIR=$PWD

mkdir build
cd build
export CXXFLAGS="-DCSA_BUILD"
export CFLAGS="-DCSA_BUILD"
scan-build cmake  .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DECW_ROOT=/opt \
  -DFileGDB_ROOT=/usr/local \
  -DGDAL_ENABLE_DRIVER_GRIB=OFF \
  -DBUILD_PYTHON_BINDINGS=OFF \
  -DBUILD_JAVA_BINDINGS=OFF \
  -DBUILD_CSHARP_BINDINGS=OFF \
  -DBUILD_TESTING=OFF

scan-build -o $GDAL_TOPDIR/scanbuildoutput -sarif -v -enable-checker alpha.unix.cstring.OutOfBounds,alpha.unix.cstring.BufferOverlap,optin.cplusplus.VirtualCall,optin.cplusplus.UninitializedObject make -j4

cd ..
