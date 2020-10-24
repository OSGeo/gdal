#!/bin/sh

set -e

export PYTHONPATH=$PWD/gdal/swig/python/build/lib.macosx-10.13-x86_64-2.7:$PWD/gdal/swig/python/build/lib.macosx-10.15-x86_64-3.8
export PYTEST="python3 -m pytest -vv -p no:sugar --color=no"

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
# tail /tmp/log.txt | grep "Failed:    0 (0 blew exceptionss)"  >/dev/null
