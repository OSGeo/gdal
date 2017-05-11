#!/bin/sh

set -e

cd gdal
export CCACHE_CPP2=yes

# Disable --with-fgdb=/usr/local since it causes /usr/local/include/GeodatabaseManagement.h:56:1: error: expected constructor, destructor, or type conversion before ‘(’ token EXT_FILEGDB_API fgdbError CreateGeodatabase(const std::wstring& path, Geodatabase& geodatabase);
# Disable --with-mongocxx=/usr/local since it should also likely be compiled with C+11, but this fails because boost itself should probably be
CC="ccache gcc-4.8 -std=c11" CXX="ccache g++-4.8 -std=c++11 -Wzero-as-null-pointer-constant -DNULL_AS_NULLPTR" ./configure --prefix=/usr --without-libtool --with-jpeg12 --with-python --with-poppler --with-podofo --with-spatialite --with-mysql --with-liblzma --with-webp --with-java --with-mdb --with-jvm-lib-add-rpath --with-epsilon --with-ecw=/usr/local --with-mrsid=/usr/local --with-mrsid-lidar=/usr/local  --with-libkml --with-openjpeg=/usr/local --with-pdfium=/usr/local --enable-pdf-plugin
# --with-gta
make USER_DEFS="-Wextra -Werror" -j3
cd apps
make USER_DEFS="-Wextra -Werror" test_ogrsf
cd ..
cd swig/java
cat java.opt | sed "s/JAVA_HOME =.*/JAVA_HOME = \/usr\/lib\/jvm\/java-7-openjdk-amd64\//" > java.opt.tmp
mv java.opt.tmp java.opt
make
cd ../..
cd swig/perl
make generate
make
cd ../..
sudo rm -f /usr/lib/libgdal.so*
sudo make install
sudo ldconfig
cd ../autotest/cpp
make -j3
cd ../../gdal
wget https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/mdb-sqlite/mdb-sqlite-1.0.2.tar.bz2
tar xjvf mdb-sqlite-1.0.2.tar.bz2
sudo cp mdb-sqlite-1.0.2/lib/*.jar /usr/lib/jvm/java-7-openjdk-amd64/jre/lib/ext
