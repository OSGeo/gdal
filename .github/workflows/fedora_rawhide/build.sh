#!/bin/sh

set -e

cmake ${GDAL_SOURCE_DIR:=..} \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_C_FLAGS="-Werror -O1 -D_FORTIFY_SOURCE=2" \
  -DCMAKE_CXX_FLAGS="-std=c++20 -Werror -O1 -D_FORTIFY_SOURCE=2" \
  -DCMAKE_SHARED_LINKER_FLAGS="-lstdc++" \
  -DUSE_CCACHE=ON \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DWERROR_DEV_FLAG="-Werror=dev"
make -j$(nproc)
make -j$(nproc) install DESTDIR=/tmp/install-gdal
