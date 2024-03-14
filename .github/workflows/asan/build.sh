#!/bin/sh

set -e

export CCACHE_CPP2=yes

if [ "$NPROC" = "" ]; then
  NPROC=3
fi

SANITIZE_FLAGS="-DMAKE_SANITIZE_HAPPY -fsanitize=undefined -fsanitize=address -fsanitize=unsigned-integer-overflow -fno-sanitize-recover=unsigned-integer-overflow"
SANITIZE_LDFLAGS="-fsanitize=undefined -fsanitize=address -shared-libasan -lstdc++"

cmake ${GDAL_SOURCE_DIR:=..} \
    -DCMAKE_BUILD_TYPE=Debug \
    -DCMAKE_C_COMPILER=clang \
    -DCMAKE_CXX_COMPILER=clang++ \
    -DCMAKE_C_FLAGS="${SANITIZE_FLAGS}" \
    -DCMAKE_CXX_FLAGS="${SANITIZE_FLAGS}" \
    -DCMAKE_SHARED_LINKER_FLAGS="${SANITIZE_LDFLAGS}" \
    -DCMAKE_EXE_LINKER_FLAGS="${SANITIZE_LDFLAGS}" \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DUSE_CCACHE=ON \
    -DGDAL_USE_GEOTIFF_INTERNAL=ON \
    -DGDAL_USE_TIFF_INTERNAL=ON \
    -DGDAL_USE_LIBKML=OFF -DOGR_ENABLE_DRIVER_LIBKML=OFF \
    -DFileGDB_ROOT=/usr/local/FileGDB_API
make -j$NPROC

