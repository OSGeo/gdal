#!/bin/bash

ret_code=0

echo "Checking for tabulation characters..."
find alg gnm port ogr gcore frmts apps \( -name "*.cpp" -o -name "*.h" \) -exec sh -c "grep -P '\t' {} >/dev/null && echo {}" \; | grep -v frmts/msg/PublicDecompWT | grep -v frmts/jpeg/libjpeg | grep -v frmts/gtiff/libtiff | grep -v frmts/gtiff/libgeotiff | grep -v frmts/grib/degrib18 | grep -v ogr/ogrsf_frmts/geojson/libjson | grep -v frmts/hdf4/hdf-eos | grep -v frmts/gif/giflib | grep -v frmts/pcraster/libcsf | grep -v frmts/png/libpng

if [[ $? -eq 0 ]] ; then
    echo "FAIL: tabulations detected. Please remove them!"
    ret_code=1
else
    echo "OK: no tabulations found."
fi

exit $ret_code
