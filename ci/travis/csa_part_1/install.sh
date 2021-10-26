#!/bin/sh

set -e

# shellcheck source=ci/travis/csa_common/install.sh
. $(dirname $0)/../csa_common/install.sh

GDAL_TOPDIR=$PWD

for dirname in port gcore frmts alg gnm ; do
    (cd $dirname; scan-build -o $GDAL_TOPDIR/scanbuildoutput -sarif -v -enable-checker alpha.unix.cstring.OutOfBounds,alpha.unix.cstring.BufferOverlap,optin.cplusplus.VirtualCall,optin.cplusplus.UninitializedObject make -j4)
done

(cd apps; scan-build -o $GDAL_TOPDIR/scanbuildoutput -sarif -v -enable-checker alpha.unix.cstring.OutOfBounds,alpha.unix.cstring.BufferOverlap,optin.cplusplus.VirtualCall,optin.cplusplus.UninitializedObject make -j4 appslib)
