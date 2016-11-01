#!/bin/sh

set -e

cd gdal
# --with-mongocxx=/usr/local
export CCACHE_CPP2=yes
CC="ccache clang" CXX="ccache clang" LDFLAGS="-lstdc++" ./configure --prefix=/usr --without-libtool --with-jpeg12 --with-python --with-poppler --with-podofo --with-spatialite --with-mysql --with-liblzma --with-webp --with-java --with-mdb --with-jvm-lib-add-rpath --with-epsilon --with-gta --with-ecw=/usr/local --with-mrsid=/usr/local --with-mrsid-lidar=/usr/local --with-fgdb=/usr/local --with-libkml --with-openjpeg=/usr/local
# Those ln -s are weird but otherwise Python bindings build fail with clang not being found
sudo ln -s /usr/local/clang-3.5.0/bin/clang /usr/bin/clang
sudo ln -s /usr/local/clang-3.5.0/bin/clang++ /usr/bin/clang++
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
