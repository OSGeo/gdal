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

export chroot="$PWD"/bionic
export LC_ALL=en_US.utf8

chroot "$chroot" ccache -M 1G
chroot "$chroot" ccache -s

chroot "$chroot" sh -c "cd $PWD && tar xzf mongo-c-driver-1.13.0.tar.gz && cd mongo-c-driver-1.13.0 && mkdir build_cmake && cd build_cmake && CCACHE_CPP2=yes CC='ccache gcc' CXX='ccache g++' cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DENABLE_TESTS=NO -DCMAKE_BUILD_TYPE=Debug && make -j3"
sudo chroot "$chroot" sh -c "cd $PWD/mongo-c-driver-1.13.0/build_cmake && make -j3 install"

chroot "$chroot" sh -c "cd $PWD && tar xzf r3.4.0.tar.gz && cd mongo-cxx-driver-r3.4.0 && mkdir build_cmake && cd build_cmake && CCACHE_CPP2=yes CC='ccache gcc' CXX='ccache g++' cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DBSONCXX_POLY_USE_BOOST=ON -DMONGOCXX_ENABLE_SLOW_TESTS=NO -DCMAKE_BUILD_TYPE=Debug && make -j3"
# -DBSONCXX_POLY_USE_MNMLSTC=OFF -DBSONCXX_POLY_USE_STD=OFF -DBSONCXX_POLY_USE_STD_EXPERIMENTAL=OFF -DBSONCXX_POLY_USE_SYSTEM_MNMLSTC=OFF
sudo chroot "$chroot" sh -c "cd $PWD/mongo-cxx-driver-r3.4.0/build_cmake && make -j3 install"

# Build libspatialite
chroot "$chroot" sh -c "cd $PWD && fossil clone https://www.gaia-gis.it/fossil/libspatialite libspatialite.fossil && mkdir sl && cd sl && fossil open ../libspatialite.fossil && CCACHE_CPP2=yes CC='ccache gcc' CXX='ccache g++' ./configure  --disable-static --prefix=/usr --disable-geos370 && CCACHE_CPP2=yes make -j3"
sudo chroot "$chroot" sh -c "cd $PWD && cd sl && make -j3 install"

# Build librasterlite2
chroot "$chroot" sh -c "cd $PWD && fossil clone https://www.gaia-gis.it/fossil/librasterlite2 librasterlite2.fossil && mkdir rl2 && cd rl2 && fossil open ../librasterlite2.fossil && CCACHE_CPP2=yes CC='ccache gcc' CXX='ccache g++' ./configure --disable-static --prefix=/usr --disable-lz4 --disable-zstd && CCACHE_CPP2=yes make -j3"
sudo chroot "$chroot" sh -c "cd $PWD && cd rl2 && make -j3 install"

# Build proj
chroot "$chroot" sh -c "cd $PWD/proj && ./autogen.sh && CC='ccache gcc' CXX='ccache g++' CFLAGS='-DPROJ_RENAME_SYMBOLS' CXXFLAGS='-DPROJ_RENAME_SYMBOLS' ./configure  --disable-static --prefix=/usr/local || cat config.log"
chroot "$chroot" sh -c "cd $PWD/proj && CCACHE_CPP2=yes make -j3"
sudo chroot "$chroot" sh -c "cd $PWD/proj && make -j3 install && mv /usr/local/lib/libproj.so.15.0.0 /usr/local/lib/libinternalproj.so.15.0.0 && rm /usr/local/lib/libproj.so*  && rm /usr/local/lib/libproj.la && ln -s libinternalproj.so.15.0.0  /usr/local/lib/libinternalproj.so.15 && ln -s libinternalproj.so.15.0.0  /usr/local/lib/libinternalproj.so"

# Configure GDAL
chroot "$chroot" sh -c "cd $PWD/gdal && CCACHE_CPP2=yes CC='ccache gcc' CXX='ccache g++' LDFLAGS='-lstdc++' ./configure --prefix=/usr --without-libtool --with-jpeg12 --with-python --with-poppler --with-spatialite --with-mysql --with-liblzma --with-webp --with-epsilon --with-proj=/usr/local --with-poppler --with-hdf5 --with-dods-root=/usr --with-sosi --with-mysql --with-rasterlite2 --with-fgdb=$PWD/FileGDB_API-64gcc51"
# --enable-debug --with-podofo

chroot "$chroot" sh -c "cd $PWD/gdal && make docs >docs_log.txt 2>&1"
chroot "$chroot" sh -c "cd $PWD/gdal && if grep -i warning docs_log.txt | grep -v -e russian -e brazilian -e setlocale -e 'has become obsolete' -e 'To avoid this warning'; then echo 'Doxygen warnings found' && cat docs_log.txt && /bin/false; else echo 'No Doxygen warnings found'; fi"
chroot "$chroot" sh -c "cd $PWD/gdal && make man >man_log.txt 2>&1"
chroot "$chroot" sh -c "cd $PWD/gdal && if grep -i warning man_log.txt | grep -v -e setlocale -e 'has become obsolete' -e 'To avoid this warning'; then echo 'Doxygen warnings found' && cat docs_log.txt && /bin/false; else echo 'No Doxygen warnings found'; fi"
chroot "$chroot" sh -c "cd $PWD/gdal && CCACHE_CPP2=yes make USER_DEFS=-Werror -j3"
chroot "$chroot" sh -c "cd $PWD/gdal/apps && make USER_DEFS=-Werror -j3 test_ogrsf"
sudo chroot "$chroot" sh -c "rm -f /usr/lib/libgdal.so*"
sudo chroot "$chroot" sh -c "cd $PWD/gdal && make install"
sudo chroot "$chroot" sh -c "sudo ldconfig"
sudo chroot "$chroot" sh -c "ln -s libgdal.so /usr/lib/libgdal.so.20"
chroot "$chroot" sh -c "cd $PWD/autotest/cpp && CCACHE_CPP2=yes make -j3"

chroot "$chroot" ccache -s
