#!/bin/sh

set -eu

if test -f /build/ccache.tar.gz; then
    echo "Restoring ccache..."
    (cd $HOME && tar xzf /build/ccache.tar.gz)
fi

cd /build

ccache -M 200M
ccache -s

cd gdal

export CC="ccache gcc"
export CXX="ccache g++"

ARCH_FLAGS=""
if (g++ -march=native -dM -E -x c++ - < /dev/null | grep AVX2 >/dev/null); then
    ARCH_FLAGS="-mavx2"
    echo "-mavx2 enabled"
    echo "Effective SSE/AVX flags:"
    g++ -mavx2 -dM -E -x c++ - < /dev/null | grep -E 'SSE|AVX'
else
    echo "AVX2 not available"
fi

CXXFLAGS="-std=c++17 -O1 $ARCH_FLAGS" CFLAGS="-O1 $ARCH_FLAGS" ./configure --prefix=/usr \
    --without-libtool \
    --with-hide-internal-symbols \
    --with-jpeg12 \
    --with-python \
    --with-poppler \
    --with-spatialite \
    --with-mysql \
    --with-liblzma \
    --with-webp \
    --with-epsilon \
    --with-hdf5 \
    --with-dods-root=/usr \
    --with-sosi \
    --with-libtiff=internal --with-rename-internal-libtiff-symbols \
    --with-geotiff=internal --with-rename-internal-libgeotiff-symbols \
    --with-kea=/usr/bin/kea-config \
    --with-tiledb \
    --with-crypto \
    --with-ecw=/opt/libecwj2-3.3 \
    --with-mrsid=/usr/local --with-jp2mrsid \
    --with-fgdb=/usr/local/FileGDB_API

make "-j$(nproc)" USER_DEFS=-Werror
(cd apps; make test_ogrsf  USER_DEFS=-Werror)
make install "-j$(nproc)"
ldconfig

(cd ../autotest/cpp && make "-j$(nproc)")

#(cd ../autotest/cpp && \
#    make vsipreload.so && \
#    LD_PRELOAD=./vsipreload.so gdalinfo /vsicurl/http://download.osgeo.org/gdal/data/ecw/spif83.ecw && \
#    LD_PRELOAD=./vsipreload.so sqlite3  /vsicurl/http://download.osgeo.org/gdal/data/sqlite3/polygon.db "select * from polygon limit 10"
#)

(cd ./swig/csharp && make generate)

# Java bindings
(cd swig/java
  cp java.opt java.opt.bak
  echo "JAVA_HOME = /usr/lib/jvm/java-8-openjdk-amd64/" | cat - java.opt.bak > java.opt
  java -version
  make
  mv java.opt.bak java.opt
)

# Perl bindings
(cd swig/perl && make generate && make)

ccache -s

#wget https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/mdb-sqlite/mdb-sqlite-1.0.2.tar.bz2
#tar xjvf mdb-sqlite-1.0.2.tar.bz2
#sudo cp mdb-sqlite-1.0.2/lib/*.jar /usr/lib/jvm/java-8-openjdk-amd64/jre/lib/ext

echo "Saving ccache..."
rm -f /build/ccache.tar.gz
(cd $HOME && tar czf /build/ccache.tar.gz .ccache)
