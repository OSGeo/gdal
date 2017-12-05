#!/bin/sh

set -e

export chroot="$PWD"/xenial
export LC_ALL=en_US.utf8

sudo chroot "$chroot" sh -c "cd $PWD/autotest/cpp && make quick_test"
# Compile and test vsipreload
sudo chroot "$chroot" sh -c "cd $PWD/autotest/cpp && make vsipreload.so"
# Run all the Python autotests

# Run ogr_fgdb test in isolation due to likely conflict with libxml2
sudo chroot "$chroot" sh -c "cd $PWD/autotest/ogr && python ogr_fgdb.py && cd ../../.."
rm autotest/ogr/ogr_fgdb.py

sudo chroot "$chroot" sh -c "cd $PWD/autotest && python run_all.py"