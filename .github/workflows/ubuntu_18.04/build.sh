#!/bin/sh

set -e

cmake ${GDAL_SOURCE_DIR:=..} \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_C_FLAGS="-Werror -DPROJ_RENAME_SYMBOLS" \
    -DCMAKE_CXX_FLAGS="-Werror -DPROJ_RENAME_SYMBOLS" \
    -DCMAKE_SHARED_LINKER_FLAGS="-lstdc++" \
    -DUSE_CCACHE=ON \
    -DPROJ_ROOT=/usr/local \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DGDAL_USE_GEOTIFF_INTERNAL:BOOL=ON \
    -DGDAL_USE_TIFF_INTERNAL:BOOL=ON
make -j3

# make install
# ldconfig
# ln -s libgdal.so /usr/lib/libgdal.so.20

## Post-install testing
#./autotest/postinstall/test_pkg-config.sh /usr
