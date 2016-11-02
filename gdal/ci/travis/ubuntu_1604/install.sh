#!/bin/sh

set -e

export chroot="$PWD"/xenial
export LC_ALL=en_US

sudo chroot "$chroot" sh -c "cd $PWD/gdal && CC=$PWD/clang+llvm-3.9.0-x86_64-linux-gnu-ubuntu-16.04/bin/clang CXX=$PWD/clang+llvm-3.9.0-x86_64-linux-gnu-ubuntu-16.04/bin/clang LDFLAGS='-lstdc++' ./configure --prefix=/usr --without-libtool --enable-debug --with-jpeg12 --with-python --with-poppler --with-podofo --with-spatialite --with-mysql --with-liblzma --with-webp --with-epsilon --with-static-proj4 --with-poppler --with-podofo --with-hdf5=/usr/lib/x86_64-linux-gnu/hdf5/serial"
sudo chroot "$chroot" sh -c "cd $PWD/gdal && make docs >docs_log.txt 2>&1"
sudo chroot "$chroot" sh -c "cd $PWD/gdal && if cat docs_log.txt | grep -i warning | grep -v russian | grep -v brazilian | grep -v setlocale | grep -v 'has become obsolete' | grep -v 'To avoid this warning'; then echo 'Doxygen warnings found' && cat docs_log.txt && /bin/false; else echo 'No Doxygen warnings found'; fi"
sudo chroot "$chroot" sh -c "cd $PWD/gdal && make man >man_log.txt 2>&1"
sudo chroot "$chroot" sh -c "cd $PWD/gdal && if cat man_log.txt | grep -i warning | grep -v setlocale | grep -v 'has become obsolete' | grep -v 'To avoid this warning'; then echo 'Doxygen warnings found' && cat docs_log.txt && /bin/false; else echo 'No Doxygen warnings found'; fi"
sudo chroot "$chroot" sh -c "cd $PWD/gdal && make USER_DEFS=-Werror -j3"
sudo chroot "$chroot" sh -c "cd $PWD/gdal/apps && make USER_DEFS=-Werror -j3 test_ogrsf"
sudo chroot "$chroot" sh -c "rm -f /usr/lib/libgdal.so*"
sudo chroot "$chroot" sh -c "cd $PWD/gdal && make install"
sudo chroot "$chroot" sh -c "sudo ldconfig"
sudo chroot "$chroot" sh -c "cd $PWD/autotest/cpp && make -j3"