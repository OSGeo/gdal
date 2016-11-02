#!/bin/sh

set -e

export chroot="$PWD"/buildroot.i386
export LC_ALL=en_US

sudo i386 chroot "$chroot" sh -c "cd $PWD/autotest/cpp && make quick_test"
# Compile and test vsipreload
sudo i386 chroot "$chroot" sh -c "cd $PWD/autotest/cpp && make vsipreload.so"
# Run all the Python autotests
sudo i386 chroot "$chroot" sh -c "cd $PWD/autotest && python run_all.py"
