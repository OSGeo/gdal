#!/bin/bash

set -e

cd gdal
# --with-mongocxx=/usr/local
export CCACHE_CPP2=yes

ccache -M 1G
ccache -s

CFLAGS=$ARCH_FLAGS CXXFLAGS=$ARCH_FLAGS CC="ccache clang" CXX="ccache clang" LDFLAGS="-lstdc++" ./configure --prefix=/usr --without-libtool --with-jpeg12 --with-python=/usr/bin/python3 --with-poppler --with-podofo --with-spatialite --with-mysql --with-liblzma --with-webp --with-java --with-mdb --with-jvm-lib-add-rpath --with-epsilon --with-ecw=/usr/local --with-mrsid=/usr/local --with-mrsid-lidar=/usr/local --with-fgdb=/usr/local --with-libkml --with-null -with-libtiff=internal
# --with-gta

# Those ln -s are weird but otherwise Python bindings build fail with clang not being found
sudo ln -s /usr/local/clang-3.5.0/bin/clang /usr/bin/clang
sudo ln -s /usr/local/clang-3.5.0/bin/clang++ /usr/bin/clang++
make USER_DEFS="-Wextra -Werror" -j3
(cd apps && make USER_DEFS="-Wextra -Werror" test_ogrsf)

(cd swig/java
  sed -i.bak "s,JAVA_HOME =.*,JAVA_HOME = /usr/lib/jvm/java-8-openjdk-amd64/," java.opt
  export JAVA_HOME=/usr/lib/jvm/java-8-openjdk-amd64
  export PATH=$JAVA_HOME/jre/bin:$PATH
  java -version
  make
  mv java.opt.bak java.opt
)

cd swig/perl
make generate
make
cd ../..
sudo rm -f /usr/lib/libgdal.so*
sudo rm -f /usr/include/gdal*.h /usr/include/ogr*.h /usr/include/gnm*.h /usr/include/cpl*.h 
sudo make install
sudo ldconfig

cd fuzzers
make USER_DEFS="-Wextra -Werror" -j3
cd tests
make USER_DEFS="-Wextra -Werror" -j3 check
cd ..
cd ..

cd ../autotest/cpp
make -j3
cd ../../gdal
wget https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/mdb-sqlite/mdb-sqlite-1.0.2.tar.bz2
tar xjvf mdb-sqlite-1.0.2.tar.bz2
sudo cp mdb-sqlite-1.0.2/lib/*.jar /usr/lib/jvm/java-7-openjdk-amd64/jre/lib/ext

ccache -s
