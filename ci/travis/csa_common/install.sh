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

export PATH=$PWD/clang+llvm-9.0.0-x86_64-linux-gnu-ubuntu-16.04/bin:$PATH
(./autogen.sh && CXXFLAGS="-std=c++11 -DCSA_BUILD" CFLAGS="-DCSA_BUILD" scan-build ./configure --prefix=/usr --without-libtool --enable-debug --with-jpeg12 --with-poppler --with-podofo --with-spatialite --with-mysql --with-liblzma --with-webp --with-java --with-mdb --with-jvm-lib-add-rpath --with-epsilon --with-ecw=/usr/local --with-fgdb=/usr/local --with-libkml --with-openjpeg=/usr/local --without-grib --with-proj=/usr/local)
