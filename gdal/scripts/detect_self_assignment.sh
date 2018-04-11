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

# This catches a bit more than gcc -Winit-self and clang -Wself-assign (https://trac.osgeo.org/gdal/ticket/6196)

echo "Checking for self assignment..."
if find alg port gcore apps ogr frmts gnm \( -name "*.cpp" -o -name "*.c" -o -name "*.h" \) -exec python scripts/detect_self_assignment.py {} \; | grep '' ; then
    echo "FAIL: check assignment detected. Please remove them!"
    ret_code=1
else
    echo "OK: no self assignment found."
fi

exit $ret_code
