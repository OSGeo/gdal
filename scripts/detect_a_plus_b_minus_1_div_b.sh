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

echo "Checking for (a + b - 1) / b..."
if grep -r -P --include='*.cpp' --include='*.hpp' --include='*.i' '\(\s*.+?\s*\+\s*(.+?)\s*-\s*1\s*\)\s*/\s*\1' alg gnm port ogr gcore frmts apps swig; then
    ret_code=1
fi

if test "$ret_code" = "1"; then
    echo "FAIL: (a + b - 1) / b patterns detected. Replace them with DIV_ROUND_UP(a, b)"
else
    echo "OK: no (a + b - 1) / b pattern found."
fi

exit $ret_code
