#!/bin/sh

set -e

export chroot="$PWD"/bionic
export LC_ALL=en_US.utf8

chroot "$chroot" ccache -M 1G
chroot "$chroot" ccache -s

chroot "$chroot" sh -c "cd $PWD && fossil clone https://www.gaia-gis.it/fossil/libspatialite libspatialite.fossil && mkdir sl && cd sl && fossil open ../libspatialite.fossil && CCACHE_CPP2=yes CC='ccache gcc' CXX='ccache g++' ./configure --prefix=/usr --disable-geos370 && CCACHE_CPP2=yes make -j3"
sudo chroot "$chroot" sh -c "cd $PWD && cd sl && make -j3 install"

chroot "$chroot" sh -c "cd $PWD && fossil clone https://www.gaia-gis.it/fossil/librasterlite2 librasterlite2.fossil && mkdir rl2 && cd rl2 && fossil open ../librasterlite2.fossil && CCACHE_CPP2=yes CC='ccache gcc' CXX='ccache g++' ./configure --prefix=/usr && CCACHE_CPP2=yes make -j3"
sudo chroot "$chroot" sh -c "cd $PWD && cd rl2 && make -j3 install"

chroot "$chroot" sh -c "cd $PWD/gdal && CCACHE_CPP2=yes CC='ccache gcc' CXX='ccache g++' LDFLAGS='-lstdc++' ./configure --prefix=/usr --without-libtool --with-jpeg12 --with-python --with-poppler --with-spatialite --with-mysql --with-liblzma --with-webp --with-epsilon --with-static-proj4 --with-poppler --with-hdf5 --with-dods-root=/usr --with-sosi --with-mysql --with-rasterlite2 --with-fgdb=$PWD/FileGDB_API-64gcc51"
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
