#!/bin/sh

set -e

SCRIPT_DIR=$(dirname "$0")
case $SCRIPT_DIR in
    "/"*)
        ;;
    ".")
        SCRIPT_DIR=$(pwd)
        ;;
    *)
        SCRIPT_DIR=$(pwd)/$(dirname "$0")
        ;;
esac
$SCRIPT_DIR/../common_install.sh

ccache -M 1G
ccache -s

# Configure GDAL
# Disable netcdf on s390x because of many test failures
sh -c "cd $PWD && mkdir build && cd build && cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DGDAL_USE_WEBP=OFF -DGDAL_USE_NETCDF=OFF -DGDAL_USE_TIFF_INTERNAL=ON -DGDAL_USE_GEOTIFF_INTERNAL=ON -DUSE_CCACHE=ON -DCMAKE_C_FLAGS='-Werror' -DCMAKE_CXX_FLAGS='-Werror'"

sh -c "cd $PWD/build && CCACHE_CPP2=yes make -j3"
sudo sh -c "rm -f /usr/lib/libgdal.so*"
sudo sh -c "cd $PWD/build && make install"
sudo sh -c "sudo ldconfig"
sudo sh -c "ln -s libgdal.so /usr/lib/libgdal.so.30"

ccache -s
