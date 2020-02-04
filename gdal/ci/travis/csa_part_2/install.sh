#!/bin/sh

set -e

# shellcheck source=gdal/ci/travis/csa_common/install.sh
. $(dirname $0)/../csa_common/install.sh

GDAL_TOPDIR=$PWD/gdal

(cd gdal &&
 make generate_gdal_version_h
 (cd ogr && scan-build -o $GDAL_TOPDIR/scanbuildoutput -plist -v -enable-checker alpha.unix.cstring.OutOfBounds,alpha.unix.cstring.BufferOverlap make -j4)
)
