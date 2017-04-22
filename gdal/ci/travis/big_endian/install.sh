#!/bin/sh

set -e

export PATH=$PWD/gcc-linaro-5.1-2015.08-x86_64_armeb-linux-gnueabihf/bin:$PATH
unset CC
unset CXX

cd gdal
export CCACHE_CPP2=yes
CC="ccache armeb-linux-gnueabihf-gcc" CXX="ccache armeb-linux-gnueabihf-g++" ./configure --host=armeb-linux-gnueabihf --without-libtool
make USER_DEFS="-Werror" -j3
cd apps
make USER_DEFS="-Werror" test_ogrsf
