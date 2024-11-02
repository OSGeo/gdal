#!/bin/sh

set -e

LD_LIBRARY_PATH="/opt/instantclient_19_9:/opt/instantclient_19_9/lib:${LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH

cmake ${GDAL_SOURCE_DIR:=..} \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS="-Werror" \
    -DCMAKE_CXX_FLAGS="-Werror" \
    -DUSE_CCACHE=ON \
    -DOracle_ROOT=/opt/instantclient_19_9 \
    -DGDAL_USE_GEOTIFF_INTERNAL:BOOL=ON \
    -DGDAL_USE_TIFF_INTERNAL:BOOL=ON

make -j$(nproc)
make -j$(nproc) install DESTDIR=/tmp/install-gdal
