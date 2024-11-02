#!/bin/bash

set -eu

CMAKE_ARGS=(
        "-DUSE_CCACHE=ON" \
        "-DCMAKE_BUILD_TYPE=Release" \
        "-DCMAKE_INSTALL_PREFIX=/usr" \
        "-DGDAL_USE_TIFF_INTERNAL=ON" \
        "-DGDAL_USE_GEOTIFF_INTERNAL=ON" \
        "-DECW_ROOT=/opt/libecwj2-3.3" \
        "-DMRSID_ROOT=/usr/local" \
        "-DFileGDB_ROOT=/usr/local/FileGDB_API" \
        "-DBUILD_CSHARP_BINDINGS=OFF" \
        "-DBUILD_JAVA_BINDINGS=OFF" \
        "-DGDAL_BUILD_OPTIONAL_DRIVERS=OFF" \
        "-DOGR_BUILD_OPTIONAL_DRIVERS=OFF" \
        "-DOGR_ENABLE_DRIVER_GPKG=ON" \
)

cmake ${GDAL_SOURCE_DIR:=..} \
    "${CMAKE_ARGS[@]}"

make -j$(nproc)

mkdir old_version
cd old_version
git clone https://github.com/OSGeo/gdal
cd gdal
git checkout 32ec2a989c083fbfd9498169052829640b7df7c6
mkdir build
cd build

cmake .. \
    "${CMAKE_ARGS[@]}"

make -j$(nproc)
