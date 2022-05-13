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
sh -c "cd $PWD && ./autogen.sh && CCACHE_CPP2=yes CC='ccache gcc' CXX='ccache g++' LDFLAGS='-lstdc++' ./configure --prefix=/usr --without-libtool --with-jpeg12 --with-python=/usr/bin/python3 --with-poppler --with-mysql --with-liblzma --without-webp --with-epsilon --with-proj=/usr/local --with-poppler --with-hdf5 --with-sosi --with-mysql --with-libtiff=internal --with-rename-internal-libtiff-symbols"

sh -c "cd $PWD && CCACHE_CPP2=yes make USER_DEFS=-Werror -j3"
sh -c "cd $PWD/apps && make USER_DEFS=-Werror -j3 test_ogrsf"
sudo sh -c "rm -f /usr/lib/libgdal.so*"
sudo sh -c "cd $PWD && make install"
sudo sh -c "sudo ldconfig"
sudo sh -c "ln -s libgdal.so /usr/lib/libgdal.so.20"
sh -c "cd $PWD/autotest/cpp && CCACHE_CPP2=yes make -j3"

ccache -s
