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

wget -q https://github.com/Kitware/CMake/releases/download/v3.22.3/cmake-3.22.3-linux-x86_64.tar.gz
tar xzf cmake-3.22.3-linux-x86_64.tar.gz
export PATH=$PWD/cmake-3.22.3-linux-x86_64/bin:$PATH

GDAL_TOPDIR=$PWD

mkdir build
cd build
export CXXFLAGS="-std=c++11 -DCSA_BUILD -DPROJ_RENAME_SYMBOLS"
export CFLAGS="-DCSA_BUILD -DPROJ_RENAME_SYMBOLS"
scan-build cmake  .. \
  -DCMAKE_BUILD_TYPE=Debug \
  -DPROJ_ROOT=/usr/local \
  -DECW_ROOT=/usr/local \
  -DFileGDB_ROOT=/usr/local \
  -DGDAL_ENABLE_DRIVER_GRIB=OFF \
  -DBUILD_PYTHON_BINDINGS=OFF \
  -DBUILD_JAVA_BINDINGS=OFF \
  -DBUILD_CSHARP_BINDINGS=OFF \
  -DBUILD_TESTING=OFF

scan-build -o $GDAL_TOPDIR/scanbuildoutput -sarif -v -enable-checker alpha.unix.cstring.OutOfBounds,alpha.unix.cstring.BufferOverlap,optin.cplusplus.VirtualCall,optin.cplusplus.UninitializedObject make -j4

cd ..
