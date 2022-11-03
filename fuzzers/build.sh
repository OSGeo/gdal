#!/bin/bash -eu
# Copyright 2016 Google Inc.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
################################################################################

# This script is meant to be run by
# https://github.com/google/oss-fuzz/blob/master/projects/gdal/Dockerfile

BUILD_SH_FROM_REPO="$SRC/gdal/fuzzers/build.sh"
if test -f "$BUILD_SH_FROM_REPO"; then
    if test "$0" != "$BUILD_SH_FROM_REPO"; then
        echo "Running $BUILD_SH_FROM_REPO"
        exec "$BUILD_SH_FROM_REPO"
        exit $?
    fi
fi

if [ "$ARCHITECTURE" = "i386" ]; then
    ARCH_SUFFIX=":i386"
else
    ARCH_SUFFIX=""
fi

if test "${CIFUZZ:-}" = "True"; then
  echo "Running under CI fuzz"

  PACKAGES="zlib1g-dev${ARCH_SUFFIX} libexpat-dev${ARCH_SUFFIX} liblzma-dev${ARCH_SUFFIX} \
          libpng-dev${ARCH_SUFFIX} libgif-dev${ARCH_SUFFIX} \
          libjpeg-dev${ARCH_SUFFIX} \
          libwebp-dev${ARCH_SUFFIX} \
          libzstd-dev${ARCH_SUFFIX} \
          libsqlite3-dev${ARCH_SUFFIX}"
  apt-get install -y $PACKAGES sqlite3

  # build libproj.a (proj master required)
  git clone --depth 1 https://github.com/OSGeo/PROJ proj
  cd proj
  cmake . -DBUILD_SHARED_LIBS:BOOL=OFF \
          -DENABLE_TIFF:BOOL=OFF \
          -DENABLE_CURL:BOOL=OFF \
          -DCMAKE_INSTALL_PREFIX=/usr \
          -DBUILD_APPS:BOOL=OFF \
          -DBUILD_TESTING:BOOL=OFF
  make -j$(nproc) -s
  make install
  cd ..

  mkdir build
  cd build
  cmake .. \
        -DCMAKE_BUILD_TYPE=Debug \
        -DBUILD_SHARED_LIBS=OFF \
        -DBUILD_TESTING=OFF -DBUILD_APPS=OFF \
        -DGDAL_BUILD_OPTIONAL_DRIVERS:BOOL=OFF \
        -DOGR_BUILD_OPTIONAL_DRIVERS:BOOL=OFF \
        -DOGR_ENABLE_DRIVER_GML:BOOL=ON \
        -DOGR_ENABLE_DRIVER_GPKG:BOOL=ON \
        -DOGR_ENABLE_DRIVER_SQLITE:BOOL=ON \
        -DGDAL_ENABLE_DRIVER_ZARR:BOOL=ON \
        -DGDAL_USE_TIFF_INTERNAL=ON \
        -DGDAL_USE_GEOTIFF_INTERNAL=ON
  make -j$(nproc) GDAL
  cd ..

  SRC_DIR=$SRC/gdal

  echo "Building gdal_fuzzer"
  $CXX $CXXFLAGS \
            -I$SRC_DIR/port -I$SRC_DIR/build/port \
            -I$SRC_DIR/gcore -I$SRC_DIR/build/gcore \
            -I$SRC_DIR/alg -I$SRC_DIR/apps -I$SRC_DIR/ogr \
            -I$SRC_DIR/ogr/ogrsf_frmts \
            -I$SRC_DIR/ogr/ogrsf_frmts/sqlite  \
            $(dirname $0)/gdal_fuzzer.cpp -o $OUT/gdal_fuzzer \
            $LIB_FUZZING_ENGINE \
            -L$SRC_DIR/build -lgdal \
            -lproj \
            -Wl,-Bstatic -lzstd -lwebp -llzma -lexpat -lsqlite3 -lgif -ljpeg -lpng -lz \
            -Wl,-Bdynamic -ldl -lpthread

  echo "Building ogr_fuzzer"
  $CXX $CXXFLAGS \
            -I$SRC_DIR/port -I$SRC_DIR/build/port \
            -I$SRC_DIR/gcore -I$SRC_DIR/build/gcore \
            -I$SRC_DIR/alg -I$SRC_DIR/apps -I$SRC_DIR/ogr \
            -I$SRC_DIR/ogr/ogrsf_frmts \
            -I$SRC_DIR/ogr/ogrsf_frmts/sqlite  \
            $(dirname $0)/ogr_fuzzer.cpp -o $OUT/ogr_fuzzer \
            $LIB_FUZZING_ENGINE \
            -L$SRC_DIR/build -lgdal \
            -L$SRC/install/lib -lproj \
            -Wl,-Bstatic -lzstd -lwebp -llzma -lexpat -lsqlite3 -lgif -ljpeg -lpng -lz \
            -Wl,-Bdynamic -ldl -lpthread

  echo "Building gdal_fuzzer_seed_corpus.zip"
  cd $(dirname $0)/../autotest/gcore/data
  rm -f $OUT/gdal_fuzzer_seed_corpus.zip
  find . -type f -exec zip -j $OUT/gdal_fuzzer_seed_corpus.zip {} \; >/dev/null
  cd $OLDPWD
  cd $(dirname $0)/../autotest/gdrivers/data
  find . -type f -exec zip -j $OUT/gdal_fuzzer_seed_corpus.zip {} \; >/dev/null
  cd $OLDPWD

  echo "Building ogr_fuzzer_seed_corpus.zip"
  CUR_DIR=$PWD
  cd $(dirname $0)/../autotest/ogr/data
  rm -f $OUT/ogr_fuzzer_seed_corpus.zip
  find . -type f -exec zip -j $OUT/ogr_fuzzer_seed_corpus.zip {} \; >/dev/null
  cd $CUR_DIR

  exit 0
fi

rm -rf proj
git clone --depth 1 https://github.com/OSGeo/PROJ proj

rm -rf curl
git clone --depth 1 https://github.com/curl/curl.git curl

rm -rf netcdf-c-4.7.4
# fix_stack_read_overflow_ncindexlookup.patch: https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=39189
curl -L https://github.com/Unidata/netcdf-c/archive/refs/tags/v4.7.4.tar.gz > v4.7.4.tar.gz && \
    tar xzf v4.7.4.tar.gz && \
    rm -f v4.7.4.tar.gz && \
    cd netcdf-c-4.7.4 && \
    patch -p0 < $SRC/gdal/fuzzers/fix_stack_read_overflow_ncindexlookup.patch && \
    cd ..

rm -rf poppler
git clone --depth 1 https://anongit.freedesktop.org/git/poppler/poppler.git poppler

# Build xerces-c from source to avoid upstream bugs
rm -rf xerces-c
git clone --depth 1 https://gitbox.apache.org/repos/asf/xerces-c.git

# Build sqlite from source to avoid upstream bugs
rm -rf sqlite
git clone --depth 1 https://github.com/sqlite/sqlite sqlite

# libxerces-c-dev${ARCH_SUFFIX}
# libsqlite3-dev${ARCH_SUFFIX}
PACKAGES="zlib1g-dev${ARCH_SUFFIX} libexpat-dev${ARCH_SUFFIX} liblzma-dev${ARCH_SUFFIX} \
          libpng-dev${ARCH_SUFFIX} libgif-dev${ARCH_SUFFIX} \
          libjpeg-dev${ARCH_SUFFIX} \
          libwebp-dev${ARCH_SUFFIX} \
          libzstd-dev${ARCH_SUFFIX} \
          libssl-dev${ARCH_SUFFIX} \
          libfreetype6-dev${ARCH_SUFFIX} libfontconfig1-dev${ARCH_SUFFIX} libtiff5-dev${ARCH_SUFFIX} libboost-dev${ARCH_SUFFIX}"

if [ "$ARCHITECTURE" = "x86_64" ]; then
  PACKAGES="${PACKAGES} libnetcdf-dev${ARCH_SUFFIX}"
fi

apt-get install -y $PACKAGES tcl

# we do not really want to deal with undefined behavior bugs in external libs, such
# as integer overflows
NON_FUZZING_CFLAGS="-O1 -fno-omit-frame-pointer -gline-tables-only"
if [ "$ARCHITECTURE" = "i386" ]; then
    NON_FUZZING_CFLAGS="-m32 ${NON_FUZZING_CFLAGS}"
fi
NON_FUZZING_CXXFLAGS="$NON_FUZZING_CFLAGS -stdlib=libc++"

# build sqlite
cd sqlite
CFLAGS="$NON_FUZZING_CFLAGS -DSQLITE_ENABLE_COLUMN_METADATA" ./configure --prefix=$SRC/install --enable-rtree --disable-tcl
make clean -s
make -j$(nproc) -s
make install
cd ..

# build poppler

# We *need* to build with the sanitize flags for the address sanitizer,
# because the C++ library is built with
# https://github.com/google/sanitizers/wiki/AddressSanitizerContainerOverflow enabled
# and we'd get false-positives (https://github.com/google/sanitizers/wiki/AddressSanitizerContainerOverflow#false-positives)
# as https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=43668 if we don't
# build GDAL's dependencies with different flags
if [ "$SANITIZER" = "address" ]; then
  POPPLER_C_FLAGS=$CFLAGS
  POPPLER_CXX_FLAGS=$CXXFLAGS
else
  POPPLER_C_FLAGS=$NON_FUZZING_CFLAGS
  POPPLER_CXX_FLAGS=$NON_FUZZING_CXXFLAGS
fi

cd poppler
mkdir -p build
cd build
cmake .. \
  -DCMAKE_INSTALL_PREFIX=$SRC/install \
  -DCMAKE_BUILD_TYPE=debug \
  -DCMAKE_C_FLAGS="$POPPLER_C_FLAGS" \
  -DCMAKE_CXX_FLAGS="$POPPLER_CXX_FLAGS" \
  -DENABLE_UNSTABLE_API_ABI_HEADERS=ON \
  -DBUILD_SHARED_LIBS=OFF \
  -DFONT_CONFIGURATION=generic \
  -DENABLE_CPP=OFF \
  -DENABLE_LIBOPENJPEG=none \
  -DENABLE_CMS=none \
  -DENABLE_LIBPNG=OFF \
  -DENABLE_LIBTIFF=OFF \
  -DENABLE_GLIB=OFF \
  -DENABLE_LIBCURL=OFF \
  -DENABLE_QT5=OFF \
  -DENABLE_UTILS=OFF \
  -DWITH_Cairo=OFF \
  -DWITH_NSS3=OFF \
  -DBUILD_CPP_TESTS=OFF \
  -DBUILD_GTK_TESTS=OFF \
  -DBUILD_MANUAL_TESTS=OFF \
  -DBUILD_QT5_TESTS=OFF

make clean -s
make -j$(nproc) -s
make install
cd ../..

# build libcurl.a (builing against Ubuntu libcurl.a doesn't work easily)
cd curl
autoreconf -fi
./configure --disable-shared --with-openssl --prefix=$SRC/install
make clean -s
make -j$(nproc) -s
make install
cd ..

# build Xerces-c
cd xerces-c
./reconf
CFLAGS=$NON_FUZZING_CFLAGS CXXFLAGS=$NON_FUZZING_CXXFLAGS ./configure --prefix=$SRC/install --with-curl=$SRC/install --without-icu
make clean -s
make -j$(nproc) -s
make install
cd ..

# build libproj.a (proj master required)
cd proj
cmake . -DBUILD_SHARED_LIBS:BOOL=OFF \
        -DSQLITE3_INCLUDE_DIR:PATH="$SRC/install/include" \
        -DSQLITE3_LIBRARY:FILEPATH="$SRC/install/lib/libsqlite3.a" \
        -DCURL_INCLUDE_DIR:PATH="$SRC/install/include" \
        -DCURL_LIBRARY_RELEASE:FILEPATH="$SRC/install/lib/libcurl.a" \
        -DCMAKE_INSTALL_PREFIX=$SRC/install \
        -DBUILD_APPS:BOOL=OFF \
        -DBUILD_TESTING:BOOL=OFF

make clean -s
make -j$(nproc) -s
make install
cd ..


if [ "$ARCHITECTURE" = "x86_64" ]; then
  # build libnetcdf.a
  cd netcdf-c-4.7.4
  mkdir -p build
  cd build
  cmake .. -DCMAKE_INSTALL_PREFIX=$SRC/install -DHDF5_C_LIBRARY=libhdf5_serial.a -DHDF5_HL_LIBRARY=libhdf5_serial_hl.a -DHDF5_INCLUDE_DIR=/usr/include/hdf5/serial -DENABLE_DAP:BOOL=OFF -DBUILD_SHARED_LIBS:BOOL=OFF -DBUILD_UTILITIES:BOOL=OFF -DBUILD_TESTING:BOOL=OFF -DENABLE_TESTS:BOOL=OFF
  make clean -s
  make -j$(nproc) -s
  make install
  cd ../..
fi

# build gdal

if [ "$SANITIZER" = "undefined" ]; then
  CFLAGS="$CFLAGS -fsanitize=unsigned-integer-overflow -fno-sanitize-recover=unsigned-integer-overflow"
  CXXFLAGS="$CXXFLAGS -fsanitize=unsigned-integer-overflow -fno-sanitize-recover=unsigned-integer-overflow"
fi

export LDFLAGS="${CXXFLAGS}"
mkdir build
cd build
export PKG_CONFIG_PATH=$SRC/install/lib/pkgconfig
cmake .. \
    -DCMAKE_PREFIX_PATH=$SRC/install \
    -DBUILD_SHARED_LIBS=OFF \
    -DGDAL_USE_TIFF_INTERNAL=ON \
    -DGDAL_USE_GEOTIFF_INTERNAL=ON \
    -DGDAL_USE_HDF5=OFF \
    -DBUILD_APPS:BOOL=OFF  \
    -DBUILD_CSHARP_BINDINGS:BOOL=OFF  \
    -DBUILD_JAVA_BINDINGS:BOOL=OFF  \
    -DBUILD_PYTHON_BINDINGS:BOOL=OFF  \
    -DBUILD_TESTING:BOOL=OFF
make -j$(nproc)
cd ..

export EXTRA_LIBS="-Wl,-Bstatic "
# curl related
export EXTRA_LIBS="$EXTRA_LIBS -L$SRC/install/lib -lcurl -lssl -lcrypto -lz"
# PROJ
export EXTRA_LIBS="$EXTRA_LIBS -lproj -ltiff "
export EXTRA_LIBS="$EXTRA_LIBS -ljbig -lzstd -lwebp -llzma -lexpat -L$SRC/install/lib -lsqlite3 -lgif -ljpeg -lpng -lz"
# Xerces-C related
export EXTRA_LIBS="$EXTRA_LIBS -L$SRC/install/lib -lxerces-c"
if [ "$ARCHITECTURE" = "x86_64" ]; then
  # netCDF related
  export EXTRA_LIBS="$EXTRA_LIBS -L$SRC/install/lib -lnetcdf -lhdf5_serial_hl -lhdf5_serial -lsz -laec -lz"
fi
# poppler related
export EXTRA_LIBS="$EXTRA_LIBS -L$SRC/install/lib -lpoppler -ljpeg -lfreetype -lfontconfig"
export EXTRA_LIBS="$EXTRA_LIBS -Wl,-Bdynamic -ldl -lpthread"

# to find sqlite3.h
export CXXFLAGS="$CXXFLAGS -I$SRC/install/include"

./fuzzers/build_google_oss_fuzzers.sh
./fuzzers/build_seed_corpus.sh
