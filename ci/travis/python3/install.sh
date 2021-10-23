#!/bin/bash

set -e

export CCACHE_CPP2=yes
export CC="ccache gcc"
export CXX="ccache g++"
ccache -M 1G
ccache -s

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
(cd proj;  ./autogen.sh && CFLAGS='-DPROJ_RENAME_SYMBOLS' CXXFLAGS='-DPROJ_RENAME_SYMBOLS' ./configure --disable-static --prefix=/usr/local && make -j3 && sudo make -j3 install && sudo mv /usr/local/lib/libproj.so.15.0.0 /usr/local/lib/libinternalproj.so.15.0.0 && sudo rm /usr/local/lib/libproj.so* && sudo rm /usr/local/lib/libproj.la && sudo ln -s libinternalproj.so.15.0.0  /usr/local/lib/libinternalproj.so.15 && sudo ln -s libinternalproj.so.15.0.0  /usr/local/lib/libinternalproj.so)

cd gdal
# --with-mongocxx=/usr/local

./autogen.sh
CFLAGS=$ARCH_FLAGS CXXFLAGS=$ARCH_FLAGS ./configure --prefix=/usr --without-libtool --with-jpeg12 --with-python=/usr/bin/python3 --with-poppler --with-podofo --with-spatialite --with-mysql --with-liblzma --with-webp --with-java --with-mdb --with-jvm-lib-add-rpath --with-epsilon --with-ecw=/usr/local  --with-fgdb=/usr/local --with-libkml --with-null -with-libtiff=internal --with-proj=/usr/local
# --with-mrsid=/usr/local --with-mrsid-lidar=/usr/local
# --with-gta

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
sudo cp mdb-sqlite-1.0.2/lib/*.jar /usr/lib/jvm/java-8-openjdk-amd64/jre/lib/ext

ccache -s
