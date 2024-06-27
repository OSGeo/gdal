#!/bin/sh

set -eu

export CXXFLAGS="-march=native -O2 -Wodr -flto-odr-type-merging -Werror"
export CFLAGS="-O2 -march=native -Werror"

cmake "${GDAL_SOURCE_DIR:=..}" \
    -DUSE_CCACHE=ON \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
    -DCMAKE_INSTALL_PREFIX=/tmp/install-gdal \
    -DGDAL_USE_TIFF_INTERNAL=OFF \
    -DGDAL_USE_GEOTIFF_INTERNAL=OFF \
    -DECW_ROOT=/opt/libecwj2-3.3 \
    -DMRSID_ROOT=/usr/local \
    -DFileGDB_ROOT=/usr/local/FileGDB_API \
    -DSQLite3_INCLUDE_DIR=/usr/local/install-sqlite-trusted-schema-off/include \
    -DSQLite3_LIBRARY=/usr/local/install-sqlite-trusted-schema-off/lib/libsqlite3.so

unset CXXFLAGS
unset CFLAGS

make "-j$(nproc)"
make "-j$(nproc)" install

# Test building MrSID driver in standalone mode
mkdir build_mrsid
cd build_mrsid
cmake -S ${GDAL_SOURCE_DIR:=..}/frmts/mrsid -DMRSID_ROOT=/usr/local -DCMAKE_PREFIX_PATH=/tmp/install-gdal
cmake --build . "-j$(nproc)"
test -f gdal_MrSID.so
cd ..
