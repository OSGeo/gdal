#!/bin/sh

set -e

cd gdal
# For libstdc++.so.6
export LD_LIBRARY_PATH=$PWD
export PATH=$PWD/install-llvm-3.7.0-light/bin:$PATH
CXXFLAGS="-std=c++11" scan-build -o scanbuildoutput -plist -v -enable-checker alpha.unix.cstring.OutOfBounds,alpha.unix.cstring.BufferOverlap ./configure --prefix=/usr --without-libtool --enable-debug --with-jpeg12 --with-poppler --with-podofo --with-spatialite --with-mysql --with-liblzma --with-webp --with-java --with-mdb --with-jvm-lib-add-rpath --with-epsilon --with-ecw=/usr/local --with-mrsid=/usr/local --with-mrsid-lidar=/usr/local --with-fgdb=/usr/local --with-libkml --with-openjpeg=/usr/local --without-grib

cd ..
