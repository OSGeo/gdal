#!/bin/sh
set -e

mkdir build
cd build

cmake \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX=$HOME/.local \
    -DGDAL_BUILD_OPTIONAL_DRIVERS=OFF \
    -DOGR_BUILD_OPTIONAL_DRIVERS=OFF \
    -DBUILD_PYTHON_BINDINGS=ON \
    -DBUILD_TESTING=OFF ..

make -j$(nproc)
make install

cd ../doc
make doxygen
