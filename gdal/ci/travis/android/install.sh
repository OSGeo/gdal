#!/bin/sh

set -e

export CCACHE_CPP2=yes

unset CC
unset CXX
export PATH=$HOME/android-toolchain/bin:$PATH

ccache -M 1G
ccache -s

cd gdal
CC="ccache arm-linux-androideabi-clang" CXX="ccache arm-linux-androideabi-clang++" CFLAGS="-mthumb" CXXFLAGS="-mthumb" ./configure --host=arm-linux-androideabi
make USER_DEFS="-Wextra -Werror" -j3

ccache -s
