#!/bin/sh

set -e

export CCACHE_CPP2=yes

unset CC
unset CXX
export PATH=$HOME/android-toolchain/bin:$PATH
export CC="ccache arm-linux-androideabi-clang"
export CXX="ccache arm-linux-androideabi-clang++"
export CFLAGS="-mthumb"
export CXXFLAGS="-mthumb"

ccache -M 1G
ccache -s

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

# build sqlite3
wget https://sqlite.org/2018/sqlite-autoconf-3250100.tar.gz
tar xzf sqlite-autoconf-3250100.tar.gz
(cd sqlite-autoconf-3250100 && ./configure --host=arm-linux-androideabi --prefix=/tmp/install && make -j3 && make install)

# Build proj
(cd proj;  ./autogen.sh && PKG_CONFIG_PATH=/tmp/install/lib/pkgconfig ./configure --host=arm-linux-androideabi --prefix=/tmp/install --disable-static && make -j3 && make install)

cd gdal
./autogen.sh
./configure --host=arm-linux-androideabi --with-proj=/tmp/install --with-sqlite3=/tmp/install
make USER_DEFS="-Wextra -Werror" -j3

ccache -s
