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

rm -rf proj
git clone --depth 1 https://github.com/OSGeo/PROJ proj

rm -rf curl
git clone --depth 1 https://github.com/curl/curl.git curl

rm -rf netcdf-4.4.1.1
curl https://src.fedoraproject.org/lookaside/pkgs/netcdf/netcdf-4.4.1.1.tar.gz/9210fd5355bee868684d9b8f83064aa6/netcdf-4.4.1.1.tar.gz > netcdf-4.4.1.1.tar.gz && \
    tar xzf netcdf-4.4.1.1.tar.gz && \
    rm -f netcdf-4.4.1.1.tar.gz && \
    mv netcdf-c-4.4.1.1 netcdf-4.4.1.1 && \
    cd netcdf-4.4.1.1 && \
    patch -p0 < $SRC/gdal/gdal/fuzzers/NC4_put_propattr_leak_fix.patch && \
    patch -p0 < $SRC/gdal/gdal/fuzzers/libnetcdf_fix_undefined_left_shift_in_ncx_get_size_t.patch && \
    cd ..

rm -rf poppler
git clone --depth 1 https://anongit.freedesktop.org/git/poppler/poppler.git poppler

# Build xerces-c from source to avoid upstream bugs
rm -rf xerces-c
git clone --depth 1 https://gitbox.apache.org/repos/asf/xerces-c.git

# Build sqlite from source to avoid upstream bugs
rm -rf sqlite
git clone --depth 1 https://github.com/sqlite/sqlite sqlite

if [ "$ARCHITECTURE" = "i386" ]; then
    ARCH_SUFFIX=":i386"
else
    ARCH_SUFFIX=""
fi

# libxerces-c-dev${ARCH_SUFFIX}
# libsqlite3-dev${ARCH_SUFFIX}
PACKAGES="zlib1g-dev${ARCH_SUFFIX} libexpat-dev${ARCH_SUFFIX} liblzma-dev${ARCH_SUFFIX} \
          libpng-dev${ARCH_SUFFIX} libgif-dev${ARCH_SUFFIX} \
          libwebp-dev${ARCH_SUFFIX} \
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
CFLAGS="$NON_FUZZING_CFLAGS -DSQLITE_ENABLE_COLUMN_METADATA" ./configure --prefix=$SRC/install --disable-tcl
make clean -s
make -j$(nproc) -s
make install
cd ..

# build poppler
cd poppler
mkdir -p build
cd build
cmake .. \
  -DCMAKE_INSTALL_PREFIX=$SRC/install \
  -DCMAKE_BUILD_TYPE=debug \
  -DCMAKE_C_FLAGS="$NON_FUZZING_CFLAGS" \
  -DCMAKE_CXX_FLAGS="$NON_FUZZING_CXXFLAGS" \
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
  -DWITH_NSS3=OFF

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
./autogen.sh
SQLITE3_CFLAGS="-I$SRC/install/include" SQLITE3_LIBS="-L$SRC/install/lib -lsqlite3" TIFF_CFLAGS=-I/usr/include TIFF_LIBS=-ltiff ./configure --disable-shared --prefix=$SRC/install --with-curl=$SRC/install/bin/curl-config
make clean -s
make -j$(nproc) -s
make install
cd ..


if [ "$ARCHITECTURE" = "x86_64" ]; then
  # build libnetcdf.a
  cd netcdf-4.4.1.1
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

cd gdal
export LDFLAGS="${CXXFLAGS}"
NETCDF_SWITCH=""
if [ "$ARCHITECTURE" = "x86_64" ]; then
  NETCDF_SWITCH="--with-netcdf=$SRC/install"
fi

PKG_CONFIG_PATH=$SRC/install/lib/pkgconfig ./configure --without-libtool --with-liblzma --with-expat --with-sqlite3=$SRC/install --with-xerces=$SRC/install --with-webp ${NETCDF_SWITCH} --with-curl=$SRC/install/bin/curl-config --without-hdf5 --with-jpeg=internal --with-proj=$SRC/install -with-proj-extra-lib-for-test="-L$SRC/install/lib -lcurl -lssl -lcrypto -lz -ltiff" --with-poppler --with-libtiff=internal --with-rename-internal-libtiff-symbols
make clean -s
make -j$(nproc) -s static-lib

export EXTRA_LIBS="-Wl,-Bstatic "
# curl related
export EXTRA_LIBS="$EXTRA_LIBS -L$SRC/install/lib -lcurl -lssl -lcrypto -lz"
# PROJ
export EXTRA_LIBS="$EXTRA_LIBS -ltiff -lproj "
export EXTRA_LIBS="$EXTRA_LIBS -lwebp -llzma -lexpat -L$SRC/install/lib -lsqlite3 -lgif -lpng -lz"
# Xerces-C related
export EXTRA_LIBS="$EXTRA_LIBS -L$SRC/install/lib -lxerces-c"
if [ "$ARCHITECTURE" = "x86_64" ]; then
  # netCDF related
  export EXTRA_LIBS="$EXTRA_LIBS -L$SRC/install/lib -lnetcdf -lhdf5_serial_hl -lhdf5_serial -lsz -laec -lz"
fi
# poppler related
export EXTRA_LIBS="$EXTRA_LIBS -L$SRC/install/lib -lpoppler -lfreetype -lfontconfig"
export EXTRA_LIBS="$EXTRA_LIBS -Wl,-Bdynamic -ldl -lpthread"

# to find sqlite3.h
export CXXFLAGS="$CXXFLAGS -I$SRC/install/include"

./fuzzers/build_google_oss_fuzzers.sh
./fuzzers/build_seed_corpus.sh
