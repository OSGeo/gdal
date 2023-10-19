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
# To be updated with a true reference branch and commit
git clone https://github.com/rouault/gdal
cd gdal
git checkout b880cad693cd6cec0b0c90422cc6430121787ce4
mkdir build
cd build

cmake .. \
    "${CMAKE_ARGS[@]}"

make -j$(nproc)
