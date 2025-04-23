#!/bin/sh

set -eu

# for precompiled headers
ccache --set-config sloppiness=pch_defines,time_macros,include_file_mtime,include_file_ctime

# Set C and C++ compiler flags to disable `_Float16`. This is
# necessary because the system C and C++ compilers don't support it,
# and Python's `build_ext` will use the system compiler to build GDAL
# Python extensions.
cmake ${GDAL_SOURCE_DIR:=..} \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_COMPILER=icx \
    -DCMAKE_CXX_COMPILER=icx \
    -DCMAKE_C_FLAGS=-DGDAL_DISABLE_FLOAT16 \
    -DCMAKE_CXX_FLAGS=-DGDAL_DISABLE_FLOAT16 \
    -DUSE_PRECOMPILED_HEADERS=ON \
    -DUSE_CCACHE=ON
make -j$(nproc)
