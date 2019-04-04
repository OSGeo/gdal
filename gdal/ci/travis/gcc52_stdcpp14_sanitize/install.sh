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

export PATH=$PWD/install-gcc-5.2.0/bin:$PATH
export LD_LIBRARY_PATH=$PWD/install-gcc-5.2.0/lib64
export PRELOAD=$PWD/install-gcc-5.2.0/lib64/libasan.so.2.0.0:$PWD/install-gcc-5.2.0/lib64/libubsan.so.0.0.0
#export PRELOAD=$PWD/install-gcc-5.2.0/lib64/libubsan.so.0.0.0
export ASAN_OPTIONS=allocator_may_return_null=1 

export CCACHE_CPP2=yes
export CC="ccache gcc"
export CXX="ccache g++"

ccache -M 1G
ccache -s

# Build proj
(cd proj;  ./autogen.sh && CFLAGS='-DPROJ_RENAME_SYMBOLS' CXXFLAGS='-DPROJ_RENAME_SYMBOLS' ./configure --disable-static --prefix=/usr/local && make -j3)
(cd proj; sudo make -j3 install && sudo ldconfig)

cd gdal

# Disable --with-fgdb=/usr/local since it causes /usr/local/include/GeodatabaseManagement.h:56:1: error: expected constructor, destructor, or type conversion before ‘(’ token EXT_FILEGDB_API fgdbError CreateGeodatabase(const std::wstring& path, Geodatabase& geodatabase);
# Disable --with-mongocxx=/usr/local since it should also likely be compiled with C+11, but this fails because boost itself should probably be
CPPFLAGS="-DMAKE_SANITIZE_HAPPY -fsanitize=undefined -fsanitize=address" LDFLAGS="-fsanitize=undefined -fsanitize=address" ./configure --with-cpp14 --prefix=/usr --without-libtool --enable-debug --with-jpeg12 --with-poppler --without-podofo --with-spatialite --with-mysql --with-liblzma --with-webp --with-epsilon --with-ecw=/usr/local  --with-libtiff=internal --with-rename-internal-libtiff-symbols --with-hide-internal-symbols --with-gnm --with-proj=/usr/local
#  --with-gta
make USER_DEFS="-Werror" -j3
cd apps
make USER_DEFS="-Werror" test_ogrsf
cd ..
cd swig/python
CPPFLAGS="-fsanitize=undefined -fsanitize=address" python setup.py build
cd ../..
#cd swig/java
#cat java.opt | sed "s/JAVA_HOME =.*/JAVA_HOME = \/usr\/lib\/jvm\/java-7-openjdk-amd64\//" > java.opt.tmp
#mv java.opt.tmp java.opt
#make
#cd ../..
#cd swig/perl
#make generate
#make
#cd ../..
sudo rm -f /usr/lib/libgdal.so*
sudo make install
cd swig/python
sudo python setup.py install
cd ../..
sudo ldconfig
#g++ -Wall -DDEBUG -fPIC -g ogr/ogrsf_frmts/null/ogrnulldriver.cpp  -shared -o ogr_NULL.so -L. -lgdal -Iport -Igcore -Iogr -Iogr/ogrsf_frmts
#GDAL_DRIVER_PATH=$PWD ogr2ogr -f null null ../autotest/ogr/data/poly.shp
cd ../autotest/cpp
make -j3
cd ../../gdal
#wget https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/mdb-sqlite/mdb-sqlite-1.0.2.tar.bz2
#tar xjvf mdb-sqlite-1.0.2.tar.bz2
#sudo cp mdb-sqlite-1.0.2/lib/*.jar /usr/lib/jvm/java-7-openjdk-amd64/jre/lib/ext

ccache -s
