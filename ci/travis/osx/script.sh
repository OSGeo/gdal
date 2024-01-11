#!/bin/sh

set -e

export PROJ_NETWORK=ON

echo 'Running CPP unit tests'
(cd build && make quicktest)

echo 'Running Python unit tests'
# install test dependencies
sudo -H pip3 install -r autotest/requirements.txt

# https://github.com/rouault/gdal/runs/1300694473
# import issues of ogr_pg from ../ogr
mv autotest/utilities/test_ogr2ogr.py autotest/utilities/test_ogr2ogr.py.disabled
mv autotest/pyscripts/test_ogr2ogr_py.py autotest/pyscripts/test_ogr2ogr_py.py.disabled

# Run all the Python autotests
(cd build && ctest -V -R autotest)

# For some reason, the tests crash at process exit
# (cd autotest; $PYTEST 2>&1 | tee /tmp/log.txt || /bin/true)
# tail /tmp/log.txt | grep "Failed:    0 (0 blew exceptions)"  >/dev/null
