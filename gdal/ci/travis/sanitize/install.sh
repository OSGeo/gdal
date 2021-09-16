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

export ASAN_OPTIONS=allocator_may_return_null=1

export CCACHE_CPP2=yes
export CC="ccache $PWD/clang+llvm-9.0.0-x86_64-linux-gnu-ubuntu-16.04/bin/clang"
export CXX="ccache $PWD/clang+llvm-9.0.0-x86_64-linux-gnu-ubuntu-16.04/bin/clang++"

ccache -M 1G
ccache -s

# Build proj
(cd proj;  ./autogen.sh && CFLAGS='-DPROJ_RENAME_SYMBOLS' CXXFLAGS='-DPROJ_RENAME_SYMBOLS' ./configure --disable-static --prefix=/usr/local && make -j3)
(cd proj; sudo make -j3 install && sudo mv /usr/local/lib/libproj.so.15.0.0 /usr/local/lib/libinternalproj.so.15.0.0 && sudo rm /usr/local/lib/libproj.so*  && sudo rm /usr/local/lib/libproj.la && sudo ln -f -s libinternalproj.so.15.0.0  /usr/local/lib/libinternalproj.so.15 && sudo ln -f -s libinternalproj.so.15.0.0  /usr/local/lib/libinternalproj.so)

cd gdal

./autogen.sh
SANITIZE_FLAGS="-DMAKE_SANITIZE_HAPPY -fsanitize=undefined -fsanitize=address -fsanitize=unsigned-integer-overflow"
CFLAGS=$SANITIZE_FLAGS CXXFLAGS=$SANITIZE_FLAGS LDFLAGS="-fsanitize=undefined -fsanitize=address -lstdc++" ./configure --prefix=/usr --without-libtool --enable-debug --with-jpeg12 --with-poppler --without-podofo --with-spatialite --with-mysql --with-liblzma --with-webp --with-epsilon --with-libtiff=internal --with-rename-internal-libtiff-symbols --with-hide-internal-symbols --with-gnm --with-proj=/usr/local --with-fgdb=$PWD/../FileGDB_API-64gcc51
sed -i "s/-fsanitize=address/-fsanitize=address -shared-libasan/g" GDALmake.opt
sed -i "s/-fsanitize=unsigned-integer-overflow/-fsanitize=unsigned-integer-overflow -fno-sanitize-recover=unsigned-integer-overflow/g" GDALmake.opt
make USER_DEFS="-Werror" -j3
cd apps
make USER_DEFS="-Werror" test_ogrsf
cd ..
cd swig/python
echo "#!/bin/sh" > mycc.sh
echo "$CC -fsanitize=undefined -fsanitize=address -shared-libasan \$*" >> mycc.sh
cat mycc.sh
chmod +x mycc.sh
PATH=$PWD:$PATH CC=mycc.sh python setup.py build
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
