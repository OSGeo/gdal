#!/bin/sh

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

find port alg gcore gnm ogr frmts apps -name "*.c*" -exec python scripts/fix_container_dot_size_zero.py {} \;
