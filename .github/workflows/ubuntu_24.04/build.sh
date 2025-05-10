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
    "-DUSE_PRECOMPILED_HEADERS=ON" \
    -DOracle_ROOT=/opt/instantclient_19_9 \
    -DGDAL_USE_GEOTIFF_INTERNAL:BOOL=ON \
    -DGDAL_USE_TIFF_INTERNAL:BOOL=ON

echo "Test turning GDAL drivers off"
cp CMakeCache.txt CMakeCache.txt.bak
cmake .. -DGDAL_BUILD_OPTIONAL_DRIVERS=OFF > /dev/null
grep GDAL_ENABLE_DRIVER_BMP:BOOL=OFF CMakeCache.txt
grep GDAL_ENABLE_DRIVER_NITF:BOOL=OFF CMakeCache.txt
cmake .. -DGDAL_ENABLE_DRIVER_BMP=ON > /dev/null
grep GDAL_ENABLE_DRIVER_BMP:BOOL=ON CMakeCache.txt
grep GDAL_ENABLE_DRIVER_NITF:BOOL=OFF CMakeCache.txt
mv CMakeCache.txt.bak CMakeCache.txt

echo "Test turning OGR drivers off"
cp CMakeCache.txt CMakeCache.txt.bak
cmake .. -DOGR_BUILD_OPTIONAL_DRIVERS=OFF > /dev/null
grep OGR_ENABLE_DRIVER_SQLITE:BOOL=OFF CMakeCache.txt
grep OGR_ENABLE_DRIVER_GML:BOOL=OFF CMakeCache.txt
cmake .. -DOGR_ENABLE_DRIVER_SQLITE=ON > /dev/null
grep OGR_ENABLE_DRIVER_SQLITE:BOOL=ON CMakeCache.txt
grep OGR_ENABLE_DRIVER_GML:BOOL=OFF CMakeCache.txt
mv CMakeCache.txt.bak CMakeCache.txt

echo "Build original configuration"
cmake .. >/dev/null
make -j$(nproc)
make -j$(nproc) install DESTDIR=/tmp/install-gdal
