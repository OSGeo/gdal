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

# install pip and use it to install test dependencies
sudo i386 chroot "$chroot" sh -c "curl -sSL 'https://bootstrap.pypa.io/get-pip.py' | python"
sudo i386 chroot "$chroot" pip install -U -r "$PWD/autotest/requirements.txt"

# Run all the Python autotests
i386 chroot "$chroot" sh -c "cd $PWD/autotest && $PYTEST"

# Run Shellcheck
shellcheck -e SC2086,SC2046 $(find $PWD/gdal -name '*.sh' -a -not -name ltmain.sh)
