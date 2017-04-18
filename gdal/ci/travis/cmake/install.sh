#!/bin/sh

set -e

export chroot="$PWD"/xenial
export LC_ALL=en_US
export LC_ALL=en_US.UTF-8
export LANGUAGE=en_US.UTF-8
export LANG=en_US.UTF-8
export LC_CTYPE=en_US.UTF-8

sudo chroot "$chroot" locale-gen en_US.UTF-8
sudo chroot "$chroot" dpkg-reconfigure locales
sudo chroot "$chroot" sh -c "mkdir $PWD/cmake-gdal-debug"
sudo chroot "$chroot" sh -c "cd $PWD/cmake-gdal-debug && CC=$PWD/clang+llvm-3.9.0-x86_64-linux-gnu-ubuntu-16.04/bin/clang CXX=$PWD/clang+llvm-3.9.0-x86_64-linux-gnu-ubuntu-16.04/bin/clang LDFLAGS='-lstdc++' cmake ../gdal"

# XXX: temporary off cppcheck.
#sudo chroot "$chroot" bash -c "cd $PWD/gdal && scripts/cppcheck.sh"

sudo chroot "$chroot" sh -c "cd $PWD/cmake-gdal-debug && make docs >docs_log.txt 2>&1"
sudo chroot "$chroot" sh -c "cd $PWD/cmake-gdal-debug && if cat docs_log.txt | grep -i warning | grep -v russian | grep -v brazilian | grep -v setlocale | grep -v 'has become obsolete' | grep -v 'To avoid this warning'; then echo 'Doxygen warnings found' && cat docs_log.txt && /bin/false; else echo 'No Doxygen warnings found'; fi"
sudo chroot "$chroot" sh -c "cd $PWD/cmake-gdal-debug && make man >man_log.txt 2>&1"
sudo chroot "$chroot" sh -c "cd $PWD/cmake-gdal-debug && if cat man_log.txt | grep -i warning | grep -v setlocale | grep -v 'has become obsolete' | grep -v 'To avoid this warning'; then echo 'Doxygen warnings found' && cat docs_log.txt && /bin/false; else echo 'No Doxygen warnings found'; fi"
sudo chroot "$chroot" sh -c "cd $PWD/cmake-gdal-debug && make USER_DEFS=-Werror -j3"
sudo chroot "$chroot" sh -c "cd $PWD/cmake-gdal-debug/apps && make USER_DEFS=-Werror -j3 test_ogrsf"
sudo chroot "$chroot" sh -c "rm -f /usr/lib/libgdal.so*"
sudo chroot "$chroot" sh -c "cd $PWD/cmake-gdal-debug && make install"
sudo chroot "$chroot" sh -c "sudo ldconfig"
sudo chroot "$chroot" sh -c "cd $PWD/autotest/cpp && make -j3"
