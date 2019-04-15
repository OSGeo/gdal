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

# Build proj
(cd proj;  ./autogen.sh && CFLAGS='-DPROJ_RENAME_SYMBOLS' CXXFLAGS='-DPROJ_RENAME_SYMBOLS' ./configure --disable-static --prefix=/usr/local && make -j3)
(cd proj; sudo make -j3 install && sudo ldconfig)

# For libstdc++.so.6
export LD_LIBRARY_PATH=$PWD/gdal
export PATH=$PWD/gdal/install-llvm-3.7.0-light/bin:$PATH
(cd gdal && CXXFLAGS="-std=c++11" scan-build -o scanbuildoutput -plist -v -enable-checker alpha.unix.cstring.OutOfBounds,alpha.unix.cstring.BufferOverlap ./configure --prefix=/usr --without-libtool --enable-debug --with-jpeg12 --with-poppler --with-podofo --with-spatialite --with-mysql --with-liblzma --with-webp --with-java --with-mdb --with-jvm-lib-add-rpath --with-epsilon --with-ecw=/usr/local --with-mrsid=/usr/local --with-mrsid-lidar=/usr/local --with-fgdb=/usr/local --with-libkml --with-openjpeg=/usr/local --without-grib --with-proj=/usr/local)
