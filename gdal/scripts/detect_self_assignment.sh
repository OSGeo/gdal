#!/bin/bash

ret_code=0

# This catches a bit more than gcc -Winit-self and clang -Wself-assign (https://trac.osgeo.org/gdal/ticket/6196)

echo "Checking for self assignment..."
find alg port gcore apps ogr frmts gnm \( -name "*.cpp" -o -name "*.c" -o -name "*.h" \) -exec python scripts/detect_self_assignment.py {} \; | grep ''

if [[ $? -eq 0 ]] ; then
    echo "FAIL: check assignment detected. Please remove them!"
    ret_code=1
else
    echo "OK: no self assignment found."
fi

exit $ret_code
