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

ccache -M 1G
ccache -s

# Build proj
sh -c "cd $PWD/proj && ./autogen.sh && CC='ccache gcc' CXX='ccache g++' CFLAGS='-DPROJ_RENAME_SYMBOLS' CXXFLAGS='-DPROJ_RENAME_SYMBOLS' ./configure  --disable-static --prefix=/usr/local || cat config.log"
sh -c "cd $PWD/proj && CCACHE_CPP2=yes make -j3"
sudo sh -c "cd $PWD/proj && make -j3 install && mv /usr/local/lib/libproj.so.15.0.0 /usr/local/lib/libinternalproj.so.15.0.0 && rm /usr/local/lib/libproj.so*  && rm /usr/local/lib/libproj.la && ln -s libinternalproj.so.15.0.0  /usr/local/lib/libinternalproj.so.15 && ln -s libinternalproj.so.15.0.0  /usr/local/lib/libinternalproj.so"

# Configure GDAL
# LERC isn't big endian ready
sh -c "cd $PWD/gdal && CCACHE_CPP2=yes CC='ccache gcc' CXX='ccache g++' LDFLAGS='-lstdc++' ./configure --prefix=/usr --without-libtool --with-jpeg12 --with-python --with-poppler --with-mysql --with-liblzma --without-webp --without-lerc --with-epsilon --with-proj=/usr/local --with-poppler --with-hdf5 --with-dods-root=/usr --with-sosi --with-mysql"

sh -c "cd $PWD/gdal && CCACHE_CPP2=yes make USER_DEFS=-Werror -j3"
sh -c "cd $PWD/gdal/apps && make USER_DEFS=-Werror -j3 test_ogrsf"
sudo sh -c "rm -f /usr/lib/libgdal.so*"
sudo sh -c "cd $PWD/gdal && make install"
sudo sh -c "sudo ldconfig"
sudo sh -c "ln -s libgdal.so /usr/lib/libgdal.so.20"
sh -c "cd $PWD/autotest/cpp && CCACHE_CPP2=yes make -j3"

ccache -s
