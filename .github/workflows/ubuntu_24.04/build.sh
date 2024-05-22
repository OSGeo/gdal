#!/bin/sh

set -e

LD_LIBRARY_PATH="/opt/instantclient_19_9:/opt/instantclient_19_9/lib:${LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH

# Use by swig/python/setup.py.in
GDAL_PYTHON_CXXFLAGS="-DGDAL_BANDMAP_TYPE_CONST_SAFE"
export GDAL_PYTHON_CXXFLAGS

cmake ${GDAL_SOURCE_DIR:=..} \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS="-Werror" \
    -DCMAKE_CXX_FLAGS="-Werror -DGDAL_BANDMAP_TYPE_CONST_SAFE" \
    -DUSE_CCACHE=ON \
    -DOracle_ROOT=/opt/instantclient_19_9 \
    -DGDAL_USE_GEOTIFF_INTERNAL:BOOL=ON \
    -DGDAL_USE_TIFF_INTERNAL:BOOL=ON

make -j$(nproc)
make -j$(nproc) install DESTDIR=/tmp/install-gdal
