#!/bin/bash

set -e

cd gdal
# --with-mongocxx=/usr/local
export CCACHE_CPP2=yes

scripts/detect_tabulations.sh
scripts/detect_printf.sh
scripts/detect_self_assignment.sh
scripts/detect_suspicious_char_digit_zero.sh

ARCH_FLAGS=""
AVX2_AVAIL=1
cat /proc/cpuinfo | grep avx2 >/dev/null || AVX2_AVAIL=0
if [[ "${AVX2_AVAIL}" == "1" ]]; then
        ARCH_FLAGS="-mavx2"
        echo "AVX2 available on CPU"
else
        echo "AVX2 not available on CPU."
        cat /proc/cpuinfo  | grep flags | head -n 1
fi

CFLAGS=$ARCH_FLAGS CXXFLAGS=$ARCH_FLAGS CC="ccache clang" CXX="ccache clang" LDFLAGS="-lstdc++" ./configure --prefix=/usr --without-libtool --with-jpeg12 --with-python --with-poppler --with-podofo --with-spatialite --with-mysql --with-liblzma --with-webp --with-java --with-mdb --with-jvm-lib-add-rpath --with-epsilon --with-ecw=/usr/local --with-mrsid=/usr/local --with-mrsid-lidar=/usr/local --with-fgdb=/usr/local --with-libkml --with-null -with-libtiff=internal
# --with-gta

make docs >docs_log.txt 2>&1
if grep -i warning docs_log.txt | grep -v -e russian -e brazilian; then echo "Doxygen warnings found" && cat docs_log.txt && /bin/false; else echo "No Doxygen warnings found"; fi
make man >man_log.txt 2>&1
if grep -i warning man_log.txt ; then echo "Doxygen warnings found" && cat docs_log.txt && /bin/false; else echo "No Doxygen warnings found"; fi

# Those ln -s are weird but otherwise Python bindings build fail with clang not being found
sudo ln -s /usr/local/clang-3.5.0/bin/clang /usr/bin/clang
sudo ln -s /usr/local/clang-3.5.0/bin/clang++ /usr/bin/clang++
make USER_DEFS="-Wextra -Werror" -j3
cd apps
make USER_DEFS="-Wextra -Werror" test_ogrsf
cd ..

cd swig/java
cp java.opt java.opt.bak
cat java.opt | sed "s/JAVA_HOME =.*/JAVA_HOME = \/usr\/lib\/jvm\/java-8-openjdk-amd64\//" > java.opt.tmp
mv java.opt.tmp java.opt
export JAVA_HOME=/usr/lib/jvm/java-8-openjdk-amd64
export PATH=$JAVA_HOME/jre/bin:$PATH
java -version
make
mv java.opt.bak java.opt
cd ../..

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
