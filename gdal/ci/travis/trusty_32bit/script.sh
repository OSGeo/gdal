#!/bin/sh

set -e

export chroot="$PWD"/buildroot.i386
export LC_ALL=en_US.utf8
export PYTEST="pytest -vv -p no:sugar --color=no"

i386 chroot "$chroot" sh -c "cd $PWD/autotest/cpp && make quick_test"
# Compile and test vsipreload
i386 chroot "$chroot" sh -c "cd $PWD/autotest/cpp && make vsipreload.so"

# We can see random crashes on vsigs.py
# https://travis-ci.org/OSGeo/gdal/jobs/278582899
# I did reproduce one locally in a chroot but after I couldn't, and
# nothing showed under Valgrind...
# so removing this script for now

mv autotest/gcore/vsigs.py autotest/gcore/vsigs.py.disabled

# https://travis-ci.com/github/OSGeo/gdal/jobs/399684890
# import issues of ogr_pg from ../ogr
mv autotest/utilities/test_ogr2ogr.py autotest/utilities/test_ogr2ogr.py.disabled
mv autotest/pyscripts/test_ogr2ogr_py.py autotest/pyscripts/test_ogr2ogr_py.py.disabled

# Run all the Python autotests
i386 chroot "$chroot" sh -c "cd $PWD/autotest && $PYTEST"
