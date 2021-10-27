#!/bin/sh

set -e

export PYTHONPATH=$PWD/swig/python/build/lib.macosx-10.9-x86_64-3.8
export PYTEST="python3 -m pytest -vv -p no:sugar --color=no"
export DYLD_LIBRARY_PATH=$HOME/install-gdal/lib
export GDAL_DATA=$HOME/install-gdal/share/gdal
export PROJ_NETWORK=ON

echo 'Running CPP unit tests'
(cd autotest/cpp && make quick_test)

echo 'Running Python unit tests'
# install test dependencies
sudo -H pip3 install -U -r autotest/requirements.txt

# https://github.com/rouault/gdal/runs/1300694473
# import issues of ogr_pg from ../ogr
mv autotest/utilities/test_ogr2ogr.py autotest/utilities/test_ogr2ogr.py.disabled
mv autotest/pyscripts/test_ogr2ogr_py.py autotest/pyscripts/test_ogr2ogr_py.py.disabled

# Run all the Python autotests
cd autotest
$PYTEST

# For some reason, the tests crash at process exit
# (cd autotest; $PYTEST 2>&1 | tee /tmp/log.txt || /bin/true)
# tail /tmp/log.txt | grep "Failed:    0 (0 blew exceptions)"  >/dev/null
