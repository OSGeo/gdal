#!/bin/sh

set -e

# Disable Curl because network based tests fail since b259dc5f54cf6a1f6ba5fb5bb4484c051d1e7142
cmake ${GDAL_SOURCE_DIR:=..} \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_C_COMPILER=clang \
  -DCMAKE_CXX_COMPILER=clang++ \
  -DCMAKE_CXX_STANDARD=23 \
  -DCMAKE_C_FLAGS="-Werror -O2 -D_FORTIFY_SOURCE=2" \
  -DCMAKE_CXX_FLAGS="-Werror -O2 -D_FORTIFY_SOURCE=2" \
  -DCMAKE_SHARED_LINKER_FLAGS="-lstdc++" \
  -DUSE_CCACHE=ON \
  "-DUSE_PRECOMPILED_HEADERS=ON" \
  -DEMBED_RESOURCE_FILES=ON \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DGDAL_USE_CURL=OFF \
  -DWERROR_DEV_FLAG="-Werror=dev"
make -j$(nproc)
make -j$(nproc) install DESTDIR=/tmp/install-gdal

ctest -V -j $(nproc)

rm -rf data
cmake ${GDAL_SOURCE_DIR:=..} \
  -DUSE_ONLY_EMBEDDED_RESOURCE_FILES=ON
rm -rf /tmp/install-gdal
make -j$(nproc)
make -j$(nproc) install DESTDIR=/tmp/install-gdal
