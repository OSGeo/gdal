#!/bin/sh

set -e

export chroot="$PWD"/buildroot.i386
export LC_ALL=en_US.utf8

i386 chroot "$chroot" ccache -M 1G
i386 chroot "$chroot" ccache -s

i386 chroot "$chroot" sh -c "cd $PWD/gdal && CCACHE_CPP2=yes CC='ccache clang' CXX='ccache clang' LIBS='-lstdc++' ./configure --prefix=/usr --without-libtool --enable-debug --with-jpeg12 --with-python --with-poppler --with-podofo --with-spatialite --with-mysql --with-liblzma --with-webp --with-epsilon"
# --with-gta
i386 chroot "$chroot" sh -c "cd $PWD/gdal && CCACHE_CPP2=yes make USER_DEFS=-Werror -j3"
i386 chroot "$chroot" sh -c "cd $PWD/gdal/apps && CCACHE_CPP2=yes make USER_DEFS=-Werror -j3 test_ogrsf"
sudo i386 chroot "$chroot" sh -c "rm -f /usr/lib/libgdal.so*"
sudo i386 chroot "$chroot" sh -c "cd $PWD/gdal && make install"
sudo i386 chroot "$chroot" sh -c "sudo ldconfig"
i386 chroot "$chroot" sh -c "cd $PWD/autotest/cpp && CCACHE_CPP2=yes make -j3"

i386 chroot "$chroot" ccache -s
