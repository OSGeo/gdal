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


I386_PACKAGES="zlib1g-dev:i386 libexpat-dev:i386 liblzma-dev:i386 \
              libxerces-c-dev:i386 libpng12-dev:i386 libgif-dev:i386 \
              libwebp-dev:i386 libicu-dev:i386 libnetcdf-dev:i386 \
              libssl-dev:i386 libsqlite3-dev:i386 \
              libfreetype6-dev:i386 libfontconfig1-dev:i386 libtiff5-dev:i386"
X64_PACKAGES="zlib1g-dev libexpat-dev liblzma-dev \
              libxerces-c-dev libpng12-dev libgif-dev \
              libwebp-dev libicu-dev libnetcdf-dev \
              libssl-dev libsqlite3-dev \
              libfreetype6-dev libfontconfig1-dev libtiff5-dev"

if [ "$ARCHITECTURE" = "i386" ]; then
    apt-get install -y $I386_PACKAGES
else
    apt-get install -y $X64_PACKAGES
fi

# build poppler
cd poppler
mkdir -p build
cd build
POPPLER_CFLAGS="$CFLAGS"
POPPLER_CXXFLAGS="$CXXFLAGS"
# we do not really want to deal with Poppler undefined behavior bugs, such
# as integer overflows
if [ "$SANITIZER" = "undefined" ]; then
    if [ "$ARCHITECTURE" = "i386" ]; then
        POPPLER_CFLAGS="-m32 -O1 -fno-omit-frame-pointer -gline-tables-only -stdlib=libc++"
    else
        POPPLER_CFLAGS="-O1 -fno-omit-frame-pointer -gline-tables-only -stdlib=libc++"
    fi
    POPPLER_CXXFLAGS="$POPPLER_CFLAGS"
fi
cmake .. \
  -DCMAKE_INSTALL_PREFIX=$SRC/install \
  -DCMAKE_BUILD_TYPE=debug \
  -DCMAKE_C_FLAGS="$POPPLER_CFLAGS" \
  -DCMAKE_CXX_FLAGS="$POPPLER_CXXFLAGS" \
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
./buildconf
./configure --disable-shared --prefix=$SRC/install
make clean -s
make -j$(nproc) -s
make install
cd ..

# build libproj.a (proj master required)
cd proj
./autogen.sh
SQLITE3_CFLAGS=-I/usr/include SQLITE3_LIBS=-lsqlite3 TIFF_CFLAGS=-I/usr/include TIFF_LIBS=-ltiff ./configure --disable-shared --prefix=$SRC/install --with-curl=$SRC/install/bin/curl-config
make clean -s
make -j$(nproc) -s
make install
cd ..

# build libnetcdf.a
cd netcdf-4.4.1.1
mkdir -p build
cd build
cmake .. -DCMAKE_INSTALL_PREFIX=$SRC/install -DHDF5_C_LIBRARY=libhdf5_serial.a -DHDF5_HL_LIBRARY=libhdf5_serial_hl.a -DHDF5_INCLUDE_DIR=/usr/include/hdf5/serial -DENABLE_DAP:BOOL=OFF -DBUILD_SHARED_LIBS:BOOL=OFF -DBUILD_UTILITIES:BOOL=OFF -DBUILD_TESTING:BOOL=OFF -DENABLE_TESTS:BOOL=OFF
make clean -s
make -j$(nproc) -s
make install
cd ../..

# build gdal

if [ "$SANITIZER" = "undefined" ]; then
  CFLAGS="$CFLAGS -fsanitize=unsigned-integer-overflow -fno-sanitize-recover=unsigned-integer-overflow"
  CXXFLAGS="$CXXFLAGS -fsanitize=unsigned-integer-overflow -fno-sanitize-recover=unsigned-integer-overflow"
fi

cd gdal
export LDFLAGS=${CXXFLAGS}
PKG_CONFIG_PATH=$SRC/install/lib/pkgconfig ./configure --without-libtool --with-liblzma --with-expat --with-sqlite3 --with-xerces --with-webp --with-netcdf=$SRC/install --with-curl=$SRC/install/bin/curl-config --without-hdf5 --with-jpeg=internal --with-proj=$SRC/install -with-proj-extra-lib-for-test="-L$SRC/install/lib -lcurl -lssl -lcrypto -lz -ltiff" --with-poppler --with-libtiff=internal --with-rename-internal-libtiff-symbols
sed -i "s/POPPLER_MINOR_VERSION = 84/POPPLER_MINOR_VERSION = 85/" GDALmake.opt # temporary hack until poppler 0.85 is released
make clean -s
make -j$(nproc) -s static-lib

export EXTRA_LIBS="-Wl,-Bstatic "
# curl related
export EXTRA_LIBS="$EXTRA_LIBS -L$SRC/install/lib -lcurl -lssl -lcrypto -lz"
# PROJ
export EXTRA_LIBS="$EXTRA_LIBS -ltiff -lproj "
export EXTRA_LIBS="$EXTRA_LIBS -lwebp -llzma -lexpat -lsqlite3 -lgif -lpng12 -lz"
# Xerces-C related
export EXTRA_LIBS="$EXTRA_LIBS -lxerces-c -licuuc -licudata"
# netCDF related
export EXTRA_LIBS="$EXTRA_LIBS -L$SRC/install/lib -lnetcdf -lhdf5_serial_hl -lhdf5_serial -lsz -laec -lz"
# poppler related
export EXTRA_LIBS="$EXTRA_LIBS -L$SRC/install/lib -lpoppler -lfreetype -lfontconfig"
export EXTRA_LIBS="$EXTRA_LIBS -Wl,-Bdynamic -ldl -lpthread"
./fuzzers/build_google_oss_fuzzers.sh
./fuzzers/build_seed_corpus.sh
