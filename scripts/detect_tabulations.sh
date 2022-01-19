#!/bin/bash

set -eu

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
GDAL_ROOT=$SCRIPT_DIR/..
cd "$GDAL_ROOT"

ret_code=0

echo "Checking for tabulation characters..."
if grep -r --files-with-matches -P --include='*.h' --include='*.cpp' --include='*.cmake' '\t' cmake alg gnm port ogr gcore frmts apps | grep -v -e frmts/msg/PublicDecompWT -e frmts/jpeg/libjpeg -e frmts/gtiff/libtiff -e frmts/gtiff/libgeotiff -e frmts/grib/degrib -e ogr/ogrsf_frmts/geojson/libjson -e frmts/hdf4/hdf-eos -e frmts/gif/giflib -e frmts/pcraster/libcsf -e frmts/png/libpng -e cpl_mem_cache -e cmake/modules/thirdparty; then
    echo "FAIL: tabulations detected. Please remove them!"
    ret_code=1
else
    echo "OK: no tabulations found."
fi

exit $ret_code
