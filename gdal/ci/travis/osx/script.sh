#!/bin/sh

set -e

cd gdal
export PYTHONPATH=$PWD/swig/python/build/lib.macosx-10.12-intel-2.7:$PWD/swig/python/build/lib.macosx-10.11-x86_64-2.7

# CPP unit tests
cd ../autotest
cd cpp
GDAL_SKIP=JP2ECW make quick_test
cd ..
# Run all the Python autotests
#make -j test
python run_all.py
