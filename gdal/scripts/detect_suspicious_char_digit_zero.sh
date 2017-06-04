#!/bin/bash

ret_code=0

echo "Checking for suspicious comparisons to '0'..."
grep -r --include="*.c*" "!= '0'" alg gnm port ogr gcore frmts apps
if [[ $? -eq 0 ]] ; then
    ret_code=1
fi
grep -r --include="*.c*" "!='0'" alg gnm port ogr gcore frmts apps | grep -v libjson
if [[ $? -eq 0 ]] ; then
    ret_code=1
fi

if [[ $ret_code -eq 1 ]]; then
    echo "FAIL: suspicious comparison to '0' found. If valid, replace '0' by a symbolic constant like DIGIT_ZERO = '0' and compare to it"
else
    echo "OK: no suspicious comparison to '0' found."
fi

exit $ret_code
