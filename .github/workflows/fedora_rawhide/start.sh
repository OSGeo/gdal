#!/bin/sh

set -e

dnf upgrade -y
dnf install -y --setopt=install_weak_deps=False proj-devel
dnf install -y clang make diffutils ccache cmake \
              libxml2-devel libxslt-devel expat-devel xerces-c-devel \
              zlib-devel xz-devel libzstd-devel blosc-devel libarchive-devel \
              giflib-devel libjpeg-devel libpng-devel \
              openjpeg2-devel cfitsio-devel libwebp-devel \
              libkml-devel json-c-devel \
              geos-devel \
              sqlite-devel pcre-devel libspatialite-devel freexl-devel \
              libtiff-devel libgeotiff-devel \
              poppler-cpp-devel \
              cryptopp-devel \
              mdbtools-devel mdbtools-odbc unixODBC-devel \
              armadillo-devel qhull-devel \
              hdf-devel hdf5-devel netcdf-devel \
              mongo-cxx-driver-devel libpq-devel \
              python3-pip python3-devel python3-lxml \
              glibc-gconv-extra

USER=root
export USER

TRAVIS=yes
export TRAVIS

BUILD_NAME=fedora
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
mkdir build_fedora_rawhide
cd build_fedora_rawhide
CC=clang CXX=clang++ LDFLAGS='-lstdc++' cmake .. \
  -DCMAKE_BUILD_TYPE=Release -DUSE_CCACHE=ON -DCMAKE_INSTALL_PREFIX=/usr \
  -DCMAKE_C_FLAGS="-Werror -O1 -D_FORTIFY_SOURCE=2" \
  -DCMAKE_CXX_FLAGS="-std=c++20 -Werror -O1 -D_FORTIFY_SOURCE=2" \
  -DWERROR_DEV_FLAG="-Werror=dev"
make -j$(nproc)
make install
ldconfig
cd ..

ccache -s

echo "Saving ccache..."
rm -f "$WORK_DIR/ccache.tar.gz"
(cd $HOME/.cache && tar czf "$WORK_DIR/ccache.tar.gz" ccache)

export PYTEST="python3 -m pytest -vv -p no:sugar --color=no"

projsync --system-directory --file us_noaa_conus.tif
projsync --system-directory --file us_nga_egm96
projsync --system-directory --file ca_nrc_ntv1_can.tif

# SKIP_TESTVIRTUALMEM because it crashes on testvirtualmem on CI
# with "ERROR 1: CPLVirtualMemManagerThread: trying to write into read-only mapping"
(cd build_fedora_rawhide && SKIP_TESTVIRTUALMEM=YES make quicktest)

# install pip and use it to install test dependencies
#pip3 install -U -r autotest/requirements.txt
pip3 install -U pytest pytest-sugar pytest-env

# test_virtualmem_1 and test_virtualmem_3 fail on CI
mv autotest/gcore/virtualmem.py autotest/gcore/virtualmem.py.disabled

(cd build_fedora_rawhide/autotest && $PYTEST)

