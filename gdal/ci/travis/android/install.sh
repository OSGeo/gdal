#!/bin/sh

set -e

export CCACHE_CPP2=yes

unset CC
unset CXX
export PATH=$HOME/android-8-toolchain/bin:$PATH

cd gdal
CC="ccache arm-linux-androideabi-gcc" CXX="ccache arm-linux-androideabi-g++" CFLAGS="-mthumb" CXXFLAGS="-mthumb" LIBS="-lsupc++ -lstdc++" ./configure --host=arm-linux-androideabi
make USER_DEFS="-Wextra -Werror" -j3
