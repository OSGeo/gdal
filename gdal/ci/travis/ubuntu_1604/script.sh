#!/bin/sh

set -e

export chroot="$PWD"/xenial
export LC_ALL=en_US.utf8
export PYTEST="pytest -ra -p no:sugar --color=no"

chroot "$chroot" sh -c "cd $PWD/autotest/cpp && make quick_test"
# Compile and test vsipreload
chroot "$chroot" sh -c "cd $PWD/autotest/cpp && make vsipreload.so"

sudo chroot "$chroot" pip install -U -r "$PWD/autotest/requirements.txt"

# Run all the Python autotests

# Run ogr_fgdb test in isolation due to likely conflict with libxml2
chroot "$chroot" sh -c "cd $PWD/autotest/ogr && $PYTEST ogr_fgdb.py && cd ../../.."
rm autotest/ogr/ogr_fgdb.py

# for some reason connection to the DB requires sudo chroot
sudo chroot "$chroot" sh -c "cd $PWD/autotest/ogr && $PYTEST ogr_mssqlspatial.py && cd ../../.."

chroot "$chroot" sh -c "cd $PWD/autotest && $PYTEST"