#!/bin/sh

set -e

ccache -M 1G
ccache -s

export CC="ccache gcc"
export CXX="ccache g++"

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

# Build proj
(cd proj;  ./autogen.sh && CFLAGS='-DPROJ_RENAME_SYMBOLS' CXXFLAGS='-DPROJ_RENAME_SYMBOLS' PKG_CONFIG_PATH="/usr/local/opt/sqlite/lib/pkgconfig" ./configure --disable-static --prefix=/tmp/install && make -j3 && make -j3 install)
rm /tmp/install/lib/libproj.la
rm /tmp/install/lib/libproj.dylib
mv /tmp/install/lib/libproj.15.dylib /tmp/install/lib/libinternalproj.15.dylib
ln -s libinternalproj.15.dylib /tmp/install/lib/libinternalproj.dylib
find /tmp/install/lib

# build GDAL
cd gdal
./configure --prefix=$HOME/install-gdal --enable-debug --with-jpeg12 --with-geotiff=internal --with-png=internal --with-proj=/tmp/install --with-sqlite3=/usr/local/opt/sqlite --without-pg --without-jasper --without-webp ${WITH_EXPAT}
make USER_DEFS="-Wextra -Werror" -j3
cd apps
make USER_DEFS="-Wextra -Werror" test_ogrsf
echo "Show which shared libs got used:"
otool -L .libs/ogrinfo
cd ..
cd swig/python
python3 setup.py build
cd ../..
make install
export PATH=$HOME/install-gdal/bin:$PWD/apps/.libs:$PATH
export DYLD_LIBRARY_PATH=$HOME/install-gdal/lib:/tmp/install/lib
export GDAL_DATA=$HOME/install-gdal/share/gdal

cd ../autotest/cpp
echo $PATH
#sudo rm -rf /usr/local/Cellar/gdal/1.10.1_1/*
#sudo ln -s /usr/bin /usr/local/Cellar/gdal/1.10.1_1
#sudo ln -s /usr/lib /usr/local/Cellar/gdal/1.10.1_1
#sudo ln -s /usr/include /usr/local/Cellar/gdal/1.10.1_1
#sudo mkdir /usr/local/Cellar/gdal/1.10.1_1/share
#sudo ln -s /usr/share/gdal /usr/local/Cellar/gdal/1.10.1_1/share
gdal-config --version
gdal-config --cflags
gdal-config --libs
make -j3
cd ../../gdal

ln -s /tmp/install/lib/libinternalproj.15.dylib /tmp/install/lib/libproj.15.dylib

ccache -s

# Post-install testing
../autotest/postinstall/test_pkg-config.sh $HOME/install-gdal
