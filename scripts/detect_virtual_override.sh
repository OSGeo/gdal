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

echo "Detect both 'virtual' and 'override'..."
if grep -r --include='*.h' --include='*.cpp' "virtual.*override" alg gnm port ogr gcore frmts apps; then
    ret_code=1
fi
if test "$ret_code" = "1"; then
    echo "FAIL: both 'virtual' and 'override' detected. Remove 'virtual' in that casse"
else
    echo "OK: not both 'virtual' and 'override' found at the same time"
fi

exit $ret_code
