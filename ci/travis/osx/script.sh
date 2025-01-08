#!/bin/bash

set -e

export PROJ_NETWORK=ON

echo 'Running CPP unit tests'
(cd build && make quicktest)

echo 'Running CPP perftests'
(cd build && ctest -V -R perf)

echo 'Running Python unit tests'
# install test dependencies
sudo -H pip3 install -r autotest/requirements.txt

NPROC=$(sysctl -n hw.ncpu)
echo "NPROC=${NPROC}"

echo "otool -L build/libgdal.dylib"
otool -L build/libgdal.dylib

echo "otool -L build/swig/python/osgeo/_gdal.cpython*"
otool -L build/swig/python/osgeo/_gdal.cpython*

DYLD_LIBRARY_PATH=$PWD/build PYTHONPATH=$PWD/build/swig/python python3 -c "from osgeo import gdal"

# Run all the Python autotests
(cd build && ctest -V -R autotest -j${NPROC})

# Use time.process_time for more reliability on VMs
# dist=no is needed because pytest-benchmark doesn't like other values of dist
# and in conftest.py/pytest.ini we set by default --dist=loadgroup
BENCHMARK_OPTIONS=(
        "--dist=no" \
        "--benchmark-timer=time.process_time" \
)
(cd build && python3 -m pytest autotest/benchmark "${BENCHMARK_OPTIONS[@]}" --capture=no -ra -vv)
