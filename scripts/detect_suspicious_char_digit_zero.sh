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

echo "Checking for suspicious comparisons to '0'..."

# Detect comparisons where we'd likely want to check against nul terminating byte in the condition of a for/while loop
if grep -r --include="*.c*" "!= '0'" alg gnm port ogr gcore frmts apps | grep -v libjson ; then
    ret_code=1
fi
if grep -r --include="*.c*" "!='0'" alg gnm port ogr gcore frmts apps | grep -v libjson ; then
    ret_code=1
fi

# Detect comparisons where we'd likely want to check against nul terminating byte with a if (*pszPtr == '\0'), after interrupting a loop due to a != '\0' check
if grep -r --include="*.c*" "== '0'" alg gnm port ogr gcore frmts apps | grep '\*' ; then
    ret_code=1
fi
if grep -r --include="*.c*" "=='0'" alg gnm port ogr gcore frmts apps | grep '\*' ; then
    ret_code=1
fi


if [[ $ret_code -eq 1 ]]; then
    echo "FAIL: suspicious comparison to '0' found. If valid, replace '0' by a symbolic constant like DIGIT_ZERO = '0' and compare to it"
else
    echo "OK: no suspicious comparison to '0' found."
fi

exit $ret_code
