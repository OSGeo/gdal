#!/bin/sh

set -eu

export CXXFLAGS="--coverage"
export CFLAGS="--coverage"
export LDFLAGS="-lgcov"

cmake ${GDAL_SOURCE_DIR:=..} \
    -DUSE_CCACHE=ON \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DGDAL_USE_TIFF_INTERNAL=ON \
    -DGDAL_USE_GEOTIFF_INTERNAL=ON \
    -DECW_ROOT=/opt/libecwj2-3.3 \
    -DMRSID_ROOT=/usr/local \
    -DFileGDB_ROOT=/usr/local/FileGDB_API

make -j$(nproc)
