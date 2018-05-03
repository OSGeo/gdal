#!/bin/sh

set -e

export PYTHONPATH=$PWD/gdal/swig/python/build/lib.macosx-10.12-intel-2.7:$PWD/gdal/swig/python/build/lib.macosx-10.11-x86_64-2.7

# CPP unit tests
(cd autotest/cpp && GDAL_SKIP=JP2ECW make quick_test)

# Run all the Python autotests
(cd autotest && python run_all.py)
