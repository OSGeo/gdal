#!/bin/sh

set -eu

# Export the compilers as environment variables so that Python's
# `build_ext` uses these compilers instead of the system compilers.
# This is important because they might differ in supporting
# `_Float16`.
CC=icx
CXX=idx
export CC CXX

cmake ${GDAL_SOURCE_DIR:=..} \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=icx \
    -DCMAKE_CXX_COMPILER=icx \
    -DUSE_CCACHE=ON
make -j$(nproc)

