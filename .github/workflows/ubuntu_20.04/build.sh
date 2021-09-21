#!/bin/sh

set -eu

if test "${COVERITY_SCAN_TOKEN:-}" = ""; then
  if test -f /build/ccache.tar.gz; then
      echo "Restoring ccache..."
      (cd $HOME && tar xzf /build/ccache.tar.gz)
  fi

  ccache -M 200M
  ccache -s

  export CC="ccache gcc"
  export CXX="ccache g++"
  export CXXFLAGS="-std=c++17 -march=native -O2 -Wodr -flto-odr-type-merging"
  export CFLAGS="-O2 -march=native"
  export OTHER_SWITCHES="--enable-lto "
else
  wget -q https://scan.coverity.com/download/cxx/linux64 --post-data "token=$COVERITY_SCAN_TOKEN&project=GDAL" -O cov-analysis-linux64.tar.gz
  mkdir /tmp/cov-analysis-linux64
  tar xzf cov-analysis-linux64.tar.gz --strip 1 -C /tmp/cov-analysis-linux64
  export OTHER_SWITCHES="--enable-debug "
fi

cd /build/gdal

./autogen.sh

./configure --prefix=/usr \
    ${OTHER_SWITCHES} \
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
    --with-fgdb=/usr/local/FileGDB_API \
    --with-opencl \
    --with-pdfium=/usr

if test "${COVERITY_SCAN_TOKEN:-}" != ""; then
  /tmp/cov-analysis-linux64/bin/cov-build --dir cov-int make "-j$(nproc)"
  tar czf cov-int.tgz cov-int
  curl \
      --form token=$COVERITY_SCAN_TOKEN \
      --form email=$COVERITY_SCAN_EMAIL \
      --form file=@cov-int.tgz \
      --form version=master \
      --form description="`git rev-parse --abbrev-ref HEAD` `git rev-parse --short HEAD`" \
      https://scan.coverity.com/builds?project=GDAL
  exit 0
fi

unset CXXFLAGS
unset CFLAGS

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
