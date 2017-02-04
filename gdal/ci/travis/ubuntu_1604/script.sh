#!/bin/sh

set -e

export chroot="$PWD"/xenial
export LC_ALL=en_US

sudo chroot "$chroot" sh -c "cd $PWD/autotest/cpp && make quick_test"
# Compile and test vsipreload
sudo chroot "$chroot" sh -c "cd $PWD/autotest/cpp && make vsipreload.so"
# Run all the Python autotests
sudo chroot "$chroot" sh -c "cd $PWD/autotest && python run_all.py"