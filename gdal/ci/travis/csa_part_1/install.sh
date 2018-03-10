#!/bin/sh

set -e

. $(dirname $0)/../csa_common/install.sh

cd gdal

for dirname in port gcore frmts alg gnm; do
    cd $dirname
    scan-build -o scanbuildoutput -plist -v -enable-checker alpha.unix.cstring.OutOfBounds,alpha.unix.cstring.BufferOverlap make -j4
    cd ..
done

cd apps
scan-build -o scanbuildoutput -plist -v -enable-checker alpha.unix.cstring.OutOfBounds,alpha.unix.cstring.BufferOverlap make -j4 appslib
cd ..

cd ..
