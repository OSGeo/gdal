#!/bin/sh

set -e

export chroot="$PWD"/buildroot.i386
export LC_ALL=en_US

sudo i386 chroot "$chroot" sh -c "cd $PWD/gdal && CC=clang CXX=clang ./configure --prefix=/usr --without-libtool --enable-debug --with-jpeg12 --with-python --with-poppler --with-podofo --with-spatialite --with-mysql --with-liblzma --with-webp --with-epsilon"
# --with-gta
sudo i386 chroot "$chroot" sh -c "cd $PWD/gdal && make USER_DEFS=-Werror -j3"
sudo i386 chroot "$chroot" sh -c "cd $PWD/gdal/apps && make USER_DEFS=-Werror -j3 test_ogrsf"
sudo i386 chroot "$chroot" sh -c "rm -f /usr/lib/libgdal.so*"
sudo i386 chroot "$chroot" sh -c "cd $PWD/gdal && make install"
sudo i386 chroot "$chroot" sh -c "sudo ldconfig"
sudo i386 chroot "$chroot" sh -c "cd $PWD/autotest/cpp && make -j3"