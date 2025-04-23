#!/bin/bash

set -eu

CMAKE_ARGS=(
        "-GNinja" \
        "-DUSE_CCACHE=ON" \
        "-DCMAKE_BUILD_TYPE=RelWithDebInfo" \
        "-DCMAKE_INSTALL_PREFIX=/usr" \
        "-DGDAL_USE_TIFF_INTERNAL=ON" \
        "-DGDAL_USE_GEOTIFF_INTERNAL=ON" \
        "-DBUILD_CSHARP_BINDINGS=OFF" \
        "-DBUILD_JAVA_BINDINGS=OFF"
)

cmake ${GDAL_SOURCE_DIR:=..} \
    "${CMAKE_ARGS[@]}"

ninja

