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
sudo sh -c "cd $PWD/proj && make -j3 install"
sudo sh -c "apt-get remove -y libproj-dev"

export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Configure GDAL
sh -c "cd $PWD && mkdir build && cd build && cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DGDAL_USE_WEBP=OFF -DPROJ_ROOT=/usr/local -DGDAL_USE_TIFF_INTERNAL=ON -DGDAL_USE_GEOTIFF_INTERNAL=ON -DUSE_CCACHE=ON -DCMAKE_C_FLAGS='-Werror -DPROJ_RENAME_SYMBOLS' -DCMAKE_CXX_FLAGS='-Werror -DPROJ_RENAME_SYMBOLS'"

sh -c "cd $PWD/build && CCACHE_CPP2=yes make -j3"
sudo sh -c "rm -f /usr/lib/libgdal.so*"
sudo sh -c "cd $PWD/build && make install"
sudo sh -c "sudo ldconfig"
sudo sh -c "ln -s libgdal.so /usr/lib/libgdal.so.30"

ccache -s
