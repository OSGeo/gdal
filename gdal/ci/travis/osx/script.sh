#!/bin/sh

set -e

cd gdal
# CPP unit tests
cd ../autotest
cd cpp
GDAL_SKIP=JP2ECW make quick_test
cd ..
# Run all the Python autotests
#make -j test
python run_all.py
