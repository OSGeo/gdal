#!/bin/sh

set -e

export chroot="$PWD"/buildroot.i386
export LC_ALL=en_US.utf8

sudo i386 chroot "$chroot" sh -c "cd $PWD/autotest/cpp && make quick_test"
# Compile and test vsipreload
sudo i386 chroot "$chroot" sh -c "cd $PWD/autotest/cpp && make vsipreload.so"

# We can see random crashes on vsigs.py
# https://travis-ci.org/OSGeo/gdal/jobs/278582899
# I did reproduce one locally in a chroot but after I couldn't, and
# nothing showed under Valgrind...
# so removing this script for now

mv autotest/gcore/vsigs.py autotest/gcore/vsigs.py.disabled

# Run all the Python autotests
sudo i386 chroot "$chroot" sh -c "cd $PWD/autotest && python run_all.py"
