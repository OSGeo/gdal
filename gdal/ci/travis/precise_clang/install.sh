#!/bin/sh

set -e

cd gdal
export CCACHE_CPP2=yes

scripts/detect_tabulations.sh
scripts/detect_printf.sh
scripts/detect_self_assignment.sh

CC="ccache clang" CXX="ccache clang" ./configure --prefix=/usr --without-libtool --without-cpp11 --enable-debug --with-jpeg12 --with-python --with-poppler --with-podofo --with-spatialite --with-mysql --with-liblzma --with-webp --with-java --with-mdb --with-jvm-lib-add-rpath --with-epsilon --with-ecw=/usr/local --with-mrsid=/usr/local --with-mrsid-lidar=/usr/local --with-fgdb=/usr/local --with-libkml --with-openjpeg=/usr/local --with-mongocxx=/usr/local
#  --with-gta
make docs >docs_log.txt 2>&1
if cat docs_log.txt | grep -i warning | grep -v russian | grep -v brazilian; then echo "Doxygen warnings found" && cat docs_log.txt && /bin/false; else echo "No Doxygen warnings found"; fi
make man >man_log.txt 2>&1
if cat man_log.txt | grep -i warning; then echo "Doxygen warnings found" && cat docs_log.txt && /bin/false; else echo "No Doxygen warnings found"; fi
make USER_DEFS="-Wextra -Werror" -j3
cd apps
make USER_DEFS="-Wextra -Werror" test_ogrsf
cd ..
cd swig/java
cat java.opt | sed "s/JAVA_HOME =.*/JAVA_HOME = \/usr\/lib\/jvm\/java-7-openjdk-amd64\//" > java.opt.tmp
mv java.opt.tmp java.opt
make
git checkout java.opt
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
