#!/bin/sh

set -e

dnf install -y --setopt=install_weak_deps=False proj-devel
dnf install -y clang make diffutils ccache \
              libxml2-devel libxslt-devel expat-devel xerces-c-devel \
              zlib-devel xz-devel libzstd-devel blosc-devel \
              giflib-devel libjpeg-devel libpng-devel \
              openjpeg2-devel cfitsio-devel libwebp-devel \
              libkml-devel json-c-devel \
              geos-devel \
              sqlite-devel pcre-devel libspatialite-devel freexl-devel \
              libtiff-devel libgeotiff-devel \
              poppler-cpp-devel \
              cryptopp-devel \
              mdbtools-devel unixODBC-devel \
              armadillo-devel qhull-devel \
              hdf-devel hdf5-devel netcdf-devel \
              mongo-cxx-driver-devel libpq-devel \
              python3-pip python3-devel

USER=root
export USER

TRAVIS=yes
export TRAVIS

cd "$WORK_DIR"

if test -f "$WORK_DIR/ccache.tar.gz"; then
    echo "Restoring ccache..."
    (cd $HOME && mkdir -p .cache && cd .cache && tar xzf "$WORK_DIR/ccache.tar.gz")
fi

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

ccache -M 1G
ccache -s

# Configure GDAL
CURRENT_DIR=$PWD
cd gdal
CC='ccache clang' CXX='ccache clang++' LDFLAGS='-lstdc++' ./configure --prefix=/usr --without-libtool --with-python=/usr/bin/python3 --with-poppler --with-spatialite --with-liblzma --with-webp --with-hdf4 --with-hdf5 --with-armadillo

make USER_DEFS=-Werror -j$(nproc)
(cd apps && make USER_DEFS=-Werror -j$(nproc) test_ogrsf)
make install
ldconfig
cd "$CURRENT_DIR"
(cd autotest/cpp && make -j3)

ccache -s

echo "Saving ccache..."
rm -f "$WORK_DIR/ccache.tar.gz"
(cd $HOME/.cache && tar czf "$WORK_DIR/ccache.tar.gz" ccache)

export PYTEST="python3 -m pytest -vv -p no:sugar --color=no"

projsync --system-directory --file us_noaa_conus.tif
projsync --system-directory --file us_nga_egm96
projsync --system-directory --file ca_nrc_ntv1_can.tif

(cd autotest/cpp && make quick_test)

# install pip and use it to install test dependencies
pip3 install -U -r autotest/requirements.txt

(cd autotest && $PYTEST)

