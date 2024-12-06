#!/bin/sh

set -eu

# for precompiled headers
ccache --set-config sloppiness=pch_defines,time_macros,include_file_mtime,include_file_ctime

cmake ${GDAL_SOURCE_DIR:=..} \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=icx \
    -DCMAKE_CXX_COMPILER=icx \
    "-DUSE_PRECOMPILED_HEADERS=ON" \
    -DUSE_CCACHE=ON
make -j$(nproc)

