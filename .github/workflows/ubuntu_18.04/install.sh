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
"$SCRIPT_DIR"/../common_install.sh

ccache -M 2G
ccache -s

tar xzf mongo-c-driver-1.13.0.tar.gz && (cd mongo-c-driver-1.13.0 && mkdir build_cmake && cd build_cmake && CC='ccache gcc' CXX='ccache g++' cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DENABLE_TESTS=NO -DCMAKE_BUILD_TYPE=Debug && make -j3 && sudo make -j3 install)

# -DBSONCXX_POLY_USE_MNMLSTC=OFF -DBSONCXX_POLY_USE_STD=OFF -DBSONCXX_POLY_USE_STD_EXPERIMENTAL=OFF -DBSONCXX_POLY_USE_SYSTEM_MNMLSTC=OFF
tar xzf r3.4.0.tar.gz && (cd mongo-cxx-driver-r3.4.0 && mkdir build_cmake && cd build_cmake && CC='ccache gcc' CXX='ccache g++' cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DBSONCXX_POLY_USE_BOOST=ON -DMONGOCXX_ENABLE_SLOW_TESTS=NO -DCMAKE_BUILD_TYPE=Debug && make -j3 && sudo make -j3 install)

# Build freexl
wget http://www.gaia-gis.it/gaia-sins/freexl-sources/freexl-2.0.0-RC0.tar.gz
tar xzf freexl-2.0.0-RC0.tar.gz
(cd freexl-2.0.0-RC0 && CC='ccache gcc' CXX='ccache g++' ./configure  --disable-static --prefix=/usr && make -j3 && sudo make -j3 install)

# Build libspatialite
fossil clone https://www.gaia-gis.it/fossil/libspatialite libspatialite.fossil && mkdir sl && (cd sl && fossil open ../libspatialite.fossil && CC='ccache gcc' CXX='ccache g++' ./configure  --disable-static --prefix=/usr --disable-geos370 --disable-rttopo --disable-geos3100 --disable-geos3110 && make -j3 && sudo make -j3 install)

# Build librasterlite2
fossil clone https://www.gaia-gis.it/fossil/librasterlite2 librasterlite2.fossil && mkdir rl2 && (cd rl2 && fossil open ../librasterlite2.fossil && fossil checkout 9dd8217cb9 && CC='ccache gcc' CXX='ccache g++' ./configure --disable-static --prefix=/usr --disable-lz4 --disable-zstd && make -j3 && sudo make -j3 install)

# Build proj
(cd proj && ./autogen.sh && CC='ccache gcc' CXX='ccache g++' CFLAGS='-DPROJ_RENAME_SYMBOLS' CXXFLAGS='-DPROJ_RENAME_SYMBOLS' ./configure  --disable-static --prefix=/usr/local || cat config.log && make -j3)
sudo sh -c "cd $PWD/proj && make -j3 install"
sudo sh -c "apt-get remove -y libproj-dev"

export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Configure GDAL
mkdir build
cd build
CFLAGS="-Werror -DPROJ_RENAME_SYMBOLS" CXXFLAGS="-Werror -DPROJ_RENAME_SYMBOLS" LDFLAGS='-lstdc++' cmake .. -DUSE_CCACHE=ON -DPROJ_ROOT=/usr/local -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DGDAL_USE_GEOTIFF_INTERNAL:BOOL=ON -DGDAL_USE_TIFF_INTERNAL:BOOL=ON
make -j3
sudo rm -f /usr/lib/libgdal.so*
sudo make install
sudo ldconfig
sudo ln -s libgdal.so /usr/lib/libgdal.so.20
cd ..

ccache -s

# Post-install testing
./autotest/postinstall/test_pkg-config.sh /usr
