#!/bin/sh

set -eu

cmake ${GDAL_SOURCE_DIR:=..} \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=icx \
    -DCMAKE_CXX_COMPILER=icx \
    -DUSE_CCACHE=ON
make -j$(nproc)

