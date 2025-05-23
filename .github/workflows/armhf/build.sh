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

echo "set(CMAKE_SYSTEM_NAME Linux)" > toolchain.cmake
echo "set(CMAKE_SYSTEM_PROCESSOR arm)" >> toolchain.cmake
echo "set(CMAKE_C_COMPILER arm-linux-gnueabihf-gcc-13)" >> toolchain.cmake
echo "set(CMAKE_CXX_COMPILER arm-linux-gnueabihf-g++-13)" >> toolchain.cmake
echo "set(CMAKE_C_FLAGS \"-mfpu=neon -Werror\")" >> toolchain.cmake
echo "set(CMAKE_CXX_FLAGS \"-mfpu=neon -Werror -Wno-psabi\")" >> toolchain.cmake

# Not sure why it fails first time...
cmake ${GDAL_SOURCE_DIR:=..} --toolchain toolchain.cmake \
    "${CMAKE_ARGS[@]}" || /bin/true

cmake ${GDAL_SOURCE_DIR:=..} --toolchain toolchain.cmake \
    "${CMAKE_ARGS[@]}"

echo "Check that GDAL_ENABLE_ARM_NEON_OPTIMIZATIONS:BOOL=ON"
(grep "GDAL_ENABLE_ARM_NEON_OPTIMIZATIONS:BOOL=ON" CMakeCache.txt > /dev/null && echo "yes") || (echo "Missing" && /bin/false)

ninja

