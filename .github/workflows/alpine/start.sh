#!/bin/sh

set -e

apk add \
    linux-headers gnu-libiconv-dev \
    g++ make ccache cmake \
    proj-dev proj proj-util \
    curl-dev tiff-dev \
    zlib-dev zstd-dev xz-dev snappy-dev \
    libjpeg-turbo-dev libpng-dev openjpeg-dev libwebp-dev expat-dev libkml-dev \
    py3-numpy-dev python3-dev py3-setuptools py3-numpy py3-pip \
    poppler-dev postgresql-dev \
    openexr-dev libheif-dev xerces-c-dev geos-dev cfitsio-dev \
    netcdf-dev libaec-dev hdf5-dev freexl-dev \
    lz4-dev blosc-dev libdeflate-dev brotli-dev uriparser-dev \
    libarchive-dev \
    kealib-dev libjxl-dev \
    json-c-dev giflib-dev \
    libspatialite-dev librasterlite2-dev \
    mariadb-connector-c-dev \
    pcre2-dev \
    qhull-dev \
    unixodbc-dev \
    libpq-dev \
    libxml2-dev

USER=root
export USER

TRAVIS=yes
export TRAVIS

BUILD_NAME=alpine
export BUILD_NAME

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
mkdir -p build_ci_alpine
cd build_ci_alpine
cmake .. \
  -DCMAKE_BUILD_TYPE=Release -DUSE_CCACHE=ON -DCMAKE_INSTALL_PREFIX=/usr \
  -DIconv_INCLUDE_DIR=/usr/include/gnu-libiconv \
  -DIconv_LIBRARY=/usr/lib/libiconv.so \
  -DCMAKE_C_FLAGS=-Werror -DCMAKE_CXX_FLAGS=-Werror -DWERROR_DEV_FLAG="-Werror=dev"
make -j$(nproc)
make install
ldconfig || /bin/true
cd ..

ccache -s

echo "Saving ccache..."
rm -f "$WORK_DIR/ccache.tar.gz"
(cd $HOME/.cache && tar czf "$WORK_DIR/ccache.tar.gz" ccache)

export PYTEST="python3 -m pytest -vv -p no:sugar --color=no"

projsync --system-directory --file us_noaa_conus.tif
projsync --system-directory --file us_nga_egm96
projsync --system-directory --file ca_nrc_ntv1_can.tif

(cd build_ci_alpine && make quicktest)

# install pip and use it to install test dependencies
pip3 install -U -r autotest/requirements.txt

(cd build_ci_alpine/autotest && $PYTEST)

