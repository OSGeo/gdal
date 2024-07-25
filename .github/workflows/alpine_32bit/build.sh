#!/bin/sh

set -e

cmake ${GDAL_SOURCE_DIR:=..} \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_SHARED_LINKER_FLAGS="-lstdc++" \
  -DUSE_CCACHE=ON \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DIconv_INCLUDE_DIR=/usr/include/gnu-libiconv \
  -DIconv_LIBRARY=/usr/lib/libiconv.so \
  -DCMAKE_C_FLAGS=-Werror -DCMAKE_CXX_FLAGS=-Werror -DWERROR_DEV_FLAG="-Werror=dev"
make -j$(nproc)
