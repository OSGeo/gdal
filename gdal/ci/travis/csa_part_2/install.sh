#!/bin/sh

set -e

. $(dirname $0)/../csa_common/install.sh

cd gdal

cd ogr
scan-build -o scanbuildoutput -plist -v -enable-checker alpha.unix.cstring.OutOfBounds,alpha.unix.cstring.BufferOverlap make -j4
cd ..

cd ..
