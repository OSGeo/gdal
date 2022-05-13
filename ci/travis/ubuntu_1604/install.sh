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

export chroot="$PWD"/xenial
export LC_ALL=en_US.utf8

chroot "$chroot" ccache -M 1G
chroot "$chroot" ccache -s

chroot "$chroot" sh -c "cd $PWD && fossil clone https://www.gaia-gis.it/fossil/libspatialite libspatialite.fossil && mkdir sl && cd sl && fossil open ../libspatialite.fossil && fossil update 90180e065d && CCACHE_CPP2=yes CC='ccache $PWD/clang+llvm-3.9.0-x86_64-linux-gnu-ubuntu-16.04/bin/clang' CXX='ccache $PWD/clang+llvm-3.9.0-x86_64-linux-gnu-ubuntu-16.04/bin/clang' ./configure --disable-static --prefix=/usr --disable-geos370 && cd src/stored_procedures && make && cd ../.. && CCACHE_CPP2=yes make -j3"
sudo chroot "$chroot" sh -c "cd $PWD && cd sl && make -j3 install"

chroot "$chroot" sh -c "cd $PWD && fossil clone https://www.gaia-gis.it/fossil/librasterlite2 librasterlite2.fossil && mkdir rl2 && cd rl2 && fossil open ../librasterlite2.fossil && CCACHE_CPP2=yes CC='ccache $PWD/clang+llvm-3.9.0-x86_64-linux-gnu-ubuntu-16.04/bin/clang' CXX='ccache $PWD/clang+llvm-3.9.0-x86_64-linux-gnu-ubuntu-16.04/bin/clang' ./configure --disable-static --prefix=/usr --disable-lz4 --disable-zstd && CCACHE_CPP2=yes make -j3"
sudo chroot "$chroot" sh -c "cd $PWD && cd rl2 && make -j3 install"

# Build proj
chroot "$chroot" sh -c "cd $PWD/proj && ./autogen.sh && CC='ccache $PWD/clang+llvm-3.9.0-x86_64-linux-gnu-ubuntu-16.04/bin/clang' CXX='ccache $PWD/clang+llvm-3.9.0-x86_64-linux-gnu-ubuntu-16.04/bin/clang' CFLAGS='-DPROJ_RENAME_SYMBOLS' CXXFLAGS='-DPROJ_RENAME_SYMBOLS' LDFLAGS='-lstdc++' ./configure --disable-static --prefix=/usr/local && CCACHE_CPP2=yes make -j3"
sudo chroot "$chroot" sh -c "cd $PWD/proj && make -j3 install && mv /usr/local/lib/libproj.so.15.0.0 /usr/local/lib/libinternalproj.so.15.0.0 && rm /usr/local/lib/libproj.so*  && rm /usr/local/lib/libproj.la && ln -s libinternalproj.so.15.0.0  /usr/local/lib/libinternalproj.so.15 && ln -s libinternalproj.so.15.0.0  /usr/local/lib/libinternalproj.so"

chroot "$chroot" sh -c "cd $PWD/gdal && ./autogen.sh && CCACHE_CPP2=yes CC='ccache $PWD/clang+llvm-3.9.0-x86_64-linux-gnu-ubuntu-16.04/bin/clang' CXX='ccache $PWD/clang+llvm-3.9.0-x86_64-linux-gnu-ubuntu-16.04/bin/clang' LDFLAGS='-lstdc++' ./configure --prefix=/usr --without-libtool --enable-debug --with-jpeg12 --with-python --with-poppler --with-podofo --with-spatialite --with-mysql --with-liblzma --with-webp --with-epsilon --with-proj=/usr/local --with-poppler --with-podofo --with-hdf5 --with-sosi --with-mysql --with-rasterlite2 --with-fgdb=$PWD/FileGDB_API-64gcc51 --with-pdfium"

chroot "$chroot" sh -c "cd $PWD/gdal && make doxygen >docs_log.txt 2>&1"
chroot "$chroot" sh -c "cd $PWD/gdal && if grep -i warning docs_log.txt | grep -v -e russian -e brazilian -e setlocale -e 'has become obsolete' -e 'To avoid this warning'; then echo 'Doxygen warnings found' && cat docs_log.txt && /bin/false; else echo 'No Doxygen warnings found'; fi"
#chroot "$chroot" sh -c "cd $PWD/gdal && make man >man_log.txt 2>&1"
#chroot "$chroot" sh -c "cd $PWD/gdal && if grep -i warning man_log.txt | grep -v -e setlocale -e 'has become obsolete' -e 'To avoid this warning'; then echo 'Doxygen warnings found' && cat docs_log.txt && /bin/false; else echo 'No Doxygen warnings found'; fi"
chroot "$chroot" sh -c "cd $PWD/gdal && CCACHE_CPP2=yes make USER_DEFS=-Werror -j3"
chroot "$chroot" sh -c "cd $PWD/gdal/apps && make USER_DEFS=-Werror -j3 test_ogrsf"
sudo chroot "$chroot" sh -c "rm -f /usr/lib/libgdal.so*"
sudo chroot "$chroot" sh -c "cd $PWD/gdal && make install"
sudo chroot "$chroot" sh -c "sudo ldconfig"
chroot "$chroot" sh -c "cd $PWD/autotest/cpp && CCACHE_CPP2=yes make -j3"

chroot "$chroot" ccache -s
