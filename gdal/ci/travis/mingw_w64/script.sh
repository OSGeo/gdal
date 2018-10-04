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

export PYTHON_DIR="$HOME/.wine/drive_c/Python27"

# install test dependencies
curl https://bootstrap.pypa.io/get-pip.py -o get-pip.py
wine64 "$PYTHON_DIR/python.exe" get-pip.py
rm get-pip.py
wine64 "$PYTHON_DIR/Scripts/pip2.7.exe" install -U -r ./requirements.txt
export PYTEST="wine64 $PYTHON_DIR/Scripts/pytest.exe -v -p no:sugar --color=no"


# Run all the Python autotests
GDAL_DATA=$PWD/../gdal/data \
    PYTHONPATH=$PWD/../gdal/swig/python/build/lib.win-amd64-2.7 \
    PATH=$PWD/../gdal:$PWD/../gdal/apps/.libs:$PWD:$PATH \
    $PYTEST
