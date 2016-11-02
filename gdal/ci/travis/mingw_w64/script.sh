#!/bin/sh

set -e

cd gdal

wine64 apps/gdalinfo.exe --version
cd ../autotest
# Does not work under wine
rm gcore/gdal_api_proxy.py
rm gcore/rfc30.py

# For some reason this crashes in the matrix .travis.yml but not in standalone branch
rm pyscripts/test_gdal2tiles.py

# Run all the Python autotests
GDAL_DATA=$PWD/../gdal/data PYTHONPATH=$PWD/../gdal/swig/python/build/lib.win-amd64-2.7 PATH=$PWD/../gdal:$PWD/../gdal/apps/.libs:$PWD:$PATH $HOME/.wine/drive_c/Python27/python.exe run_all.py
