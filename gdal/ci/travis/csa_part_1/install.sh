#!/bin/sh

set -e

. $(dirname $0)/../csa_common/install.sh

for dirname in gdal/port gdal/gcore gdal/frmts gdal/alg gdal/gnm ; do
    (cd $dirname; scan-build -o scanbuildoutput -plist -v -enable-checker alpha.unix.cstring.OutOfBounds,alpha.unix.cstring.BufferOverlap make -j4)
done

(cd gdal/apps; scan-build -o scanbuildoutput -plist -v -enable-checker alpha.unix.cstring.OutOfBounds,alpha.unix.cstring.BufferOverlap make -j4 appslib)
