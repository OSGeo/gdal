#!/bin/sh

set -e

SCRIPT_DIR=$(dirname "$0")
case $SCRIPT_DIR in
    "/"*)
        ;;
    ".")
        SCRIPT_DIR=$(pwd)
        ;;
    *)
        SCRIPT_DIR=$(pwd)/$(dirname "$0")
        ;;
esac
$SCRIPT_DIR/../common_install.sh

export chroot="$PWD"/buildroot.i386
export LC_ALL=en_US.utf8

i386 chroot "$chroot" ccache -M 1G
i386 chroot "$chroot" ccache -s

# Build proj
i386 chroot "$chroot" sh -c "cd $PWD/proj && ./autogen.sh && CC='ccache gcc' CXX='ccache g++' CFLAGS='-DPROJ_RENAME_SYMBOLS' CXXFLAGS='-DPROJ_RENAME_SYMBOLS' ./configure --disable-static --prefix=/usr/local && CCACHE_CPP2=yes make -j3"
sudo i386 chroot "$chroot" sh -c "cd $PWD/proj && make -j3 install && mv /usr/local/lib/libproj.so.15.0.0 /usr/local/lib/libinternalproj.so.15.0.0 && rm /usr/local/lib/libproj.* && ln -s libinternalproj.so.15.0.0  /usr/local/lib/libinternalproj.so.15 && ln -s libinternalproj.so.15.0.0  /usr/local/lib/libinternalproj.so"

i386 chroot "$chroot" sh -c "cd $PWD/gdal && ./autogen.sh && CCACHE_CPP2=yes CC='ccache clang' CXX='ccache clang' LIBS='-lstdc++' ./configure --prefix=/usr --without-libtool --enable-debug --with-jpeg12 --with-python=/usr/bin/python3.5 --with-poppler --with-podofo --with-spatialite --with-mysql --with-liblzma --with-webp --with-epsilon --with-proj=/usr/local"
# --with-gta
i386 chroot "$chroot" sh -c "cd $PWD/gdal && CCACHE_CPP2=yes make USER_DEFS=-Werror -j3"
i386 chroot "$chroot" sh -c "cd $PWD/gdal/apps && CCACHE_CPP2=yes make USER_DEFS=-Werror -j3 test_ogrsf"
sudo i386 chroot "$chroot" sh -c "rm -f /usr/lib/libgdal.so*"
sudo i386 chroot "$chroot" sh -c "cd $PWD/gdal && make install"
sudo i386 chroot "$chroot" sh -c "sudo ldconfig"
i386 chroot "$chroot" sh -c "cd $PWD/autotest/cpp && CCACHE_CPP2=yes make -j3"

i386 chroot "$chroot" ccache -s
