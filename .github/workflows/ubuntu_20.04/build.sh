#!/bin/sh

set -eu

cd /build

cd gdal

CXXFLAGS="-std=c++17 -O1" ./configure --prefix=/usr \
    --without-libtool \
    --with-hide-internal-symbols \
    --with-jpeg12 \
    --with-python \
    --with-poppler \
    --with-spatialite \
    --with-mysql \
    --with-liblzma \
    --with-webp \
    --with-epsilon \
    --with-hdf5 \
    --with-dods-root=/usr \
    --with-sosi \
    --with-libtiff=internal --with-rename-internal-libtiff-symbols \
    --with-geotiff=internal --with-rename-internal-libgeotiff-symbols \
    --with-kea=/usr/bin/kea-config \
    --with-tiledb \
    --with-crypto

make "-j$(nproc)" USER_DEFS=-Werror
(cd apps; make test_ogrsf  USER_DEFS=-Werror)
make install "-j$(nproc)"
ldconfig

(cd ../autotest/cpp && make "-j$(nproc)")

(cd ./swig/csharp && make generate)
