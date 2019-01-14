#!/bin/sh

set -e

export PYTHONPATH=$PWD/gdal/swig/python/build/lib.macosx-10.12-intel-2.7:$PWD/gdal/swig/python/build/lib.macosx-10.11-x86_64-2.7:$PWD/gdal/swig/python/build/lib.macosx-10.12-x86_64-2.7
export PYTEST="pytest -vv -p no:sugar --color=no"

echo 'Running CPP unit tests'
(cd autotest/cpp && GDAL_SKIP=JP2ECW make quick_test)

echo 'Running Python unit tests'
# install test dependencies
sudo -H pip install -U -r autotest/requirements.txt

# Run all the Python autotests
cd autotest
$PYTEST

# For some reason, the tests crash at process exit
# (cd autotest; $PYTEST 2>&1 | tee /tmp/log.txt || /bin/true)
# tail /tmp/log.txt | grep "Failed:    0 (0 blew exceptionss)"  >/dev/null
