#!/bin/sh

set -e

cmake ${GDAL_SOURCE_DIR:=..} \
    -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    -DCMAKE_C_FLAGS="-Werror -Wno-psabi" \
    -DCMAKE_CXX_FLAGS="-Werror -Wno-psabi" \
    -DUSE_CCACHE=ON \
    -DUSE_PRECOMPILED_HEADERS=ON \
    -DLLVM_FIND_VERSION=21 \
    -DGDAL_USE_GEOTIFF_INTERNAL:BOOL=ON \
    -DGDAL_USE_TIFF_INTERNAL:BOOL=ON

grep "GDAL_USE_LLVM:BOOL=ON" CMakeCache.txt >/dev/null || (echo "LLVM not enabled"; grep LLVM CMakeCache.txt; /bin/false)

cmake .. >/dev/null
make -j$(nproc)
