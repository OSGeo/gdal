#!/bin/sh

set -e

cmake ${GDAL_SOURCE_DIR:=..} \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS="-Werror" \
    -DCMAKE_CXX_FLAGS="-Werror" \
    -DUSE_CCACHE=ON \
    -DGDAL_USE_GEOTIFF_INTERNAL:BOOL=ON \
    -DGDAL_USE_TIFF_INTERNAL:BOOL=ON

make -j$(nproc)
