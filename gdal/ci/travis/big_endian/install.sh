#!/bin/sh

set -e

export PATH=$PWD/gcc-linaro-5.1-2015.08-x86_64_armeb-linux-gnueabihf/bin:$PATH
unset CC
unset CXX
export CC="ccache armeb-linux-gnueabihf-gcc"
export CXX="ccache armeb-linux-gnueabihf-g++"
export CCACHE_CPP2=yes

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
(cd sqlite-autoconf-3250100 && ./configure --host=armeb-linux-gnueabihf --prefix=/tmp/install && make -j3 && make install)

# Build proj
(cd proj;  ./autogen.sh && PKG_CONFIG_PATH=/tmp/install/lib/pkgconfig ./configure  --disable-static --host=armeb-linux-gnueabihf --prefix=/tmp/install --disable-lto && make -j3 && make install)

cd gdal
./autogen.sh
./configure --host=armeb-linux-gnueabihf --without-libtool --with-sqlite3=/tmp/install --with-proj=/tmp/install || cat config.log
make USER_DEFS="-Werror" -j3
cd apps
make USER_DEFS="-Werror" test_ogrsf
