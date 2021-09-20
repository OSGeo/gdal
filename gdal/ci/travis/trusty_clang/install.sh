#!/bin/bash

set -e

# --with-mongocxx=/usr/local
export CCACHE_CPP2=yes
export CC="ccache clang"
export CXX="ccache clang++"

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
(cd proj; ./autogen.sh && CFLAGS='-DPROJ_RENAME_SYMBOLS' CXXFLAGS='-DPROJ_RENAME_SYMBOLS' ./configure --disable-static --prefix=/usr/local && make -j3)
(cd proj; sudo make -j3 install && sudo ldconfig)

cd gdal

./autogen.sh
ARCH_FLAGS=""
AVX2_AVAIL=1
grep avx2 /proc/cpuinfo >/dev/null || AVX2_AVAIL=0
if [[ "${AVX2_AVAIL}" == "1" ]]; then
        ARCH_FLAGS="-mavx2"
        echo "AVX2 available on CPU"
else
        echo "AVX2 not available on CPU."
        grep flags /proc/cpuinfo | head -n 1
fi

CFLAGS=$ARCH_FLAGS CXXFLAGS=$ARCH_FLAGS  LDFLAGS="-lstdc++" ./configure --prefix=/usr --without-libtool --with-jpeg12 --with-python=/usr/bin/python3.5 --with-poppler --with-podofo --with-spatialite --with-mysql --with-liblzma --with-webp --with-java --with-mdb --with-jvm-lib-add-rpath --with-epsilon --with-ecw=/usr/local --with-mrsid=/usr/local --with-mrsid-lidar=/usr/local --with-fgdb=/usr/local --with-libkml --with-null -with-libtiff=internal --with-proj=/usr/local
# --with-gta

make doxygen >docs_log.txt 2>&1
if grep -i warning docs_log.txt | grep -v -e russian -e brazilian; then echo "Doxygen warnings found" && cat docs_log.txt && /bin/false; else echo "No Doxygen warnings found"; fi
#make man >man_log.txt 2>&1
#if grep -i warning man_log.txt ; then echo "Doxygen warnings found" && cat docs_log.txt && /bin/false; else echo "No Doxygen warnings found"; fi

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
  (make 2>/tmp/log.txt || cat /tmp/log.txt)
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
