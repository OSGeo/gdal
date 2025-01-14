#!/bin/sh

set -eu

export CXXFLAGS="-march=native -O2 -Wodr -flto-odr-type-merging -Werror"
export CFLAGS="-O2 -march=native -Werror"

# for precompiled headers
ccache --set-config sloppiness=pch_defines,time_macros,include_file_mtime,include_file_ctime

cmake "${GDAL_SOURCE_DIR:=..}" \
    -DUSE_CCACHE=ON \
    "-DUSE_PRECOMPILED_HEADERS=ON" \
    -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON \
    -DCMAKE_INSTALL_PREFIX=/tmp/install-gdal \
    -DGDAL_USE_TIFF_INTERNAL=OFF \
    -DGDAL_USE_GEOTIFF_INTERNAL=OFF \
    -DGDAL_USE_EXPRTK=ON \
    -DECW_ROOT=/opt/libecwj2-3.3 \
    -DMRSID_ROOT=/usr/local \
    -DFileGDB_ROOT=/usr/local/FileGDB_API \
    -DSQLite3_INCLUDE_DIR=/usr/local/install-sqlite-trusted-schema-off/include \
    -DSQLite3_LIBRARY=/usr/local/install-sqlite-trusted-schema-off/lib/libsqlite3.so

unset CXXFLAGS
unset CFLAGS

make "-j$(nproc)"
make "-j$(nproc)" install

# Download Oracle SDK
wget https://download.oracle.com/otn_software/linux/instantclient/1923000/instantclient-basic-linux.x64-19.23.0.0.0dbru.zip
wget https://download.oracle.com/otn_software/linux/instantclient/1923000/instantclient-sdk-linux.x64-19.23.0.0.0dbru.zip
unzip -o instantclient-basic-linux.x64-19.23.0.0.0dbru.zip
unzip -o instantclient-sdk-linux.x64-19.23.0.0.0dbru.zip

# Test building MrSID driver in standalone mode
mkdir build_mrsid
cd build_mrsid
cmake -S ${GDAL_SOURCE_DIR:=..}/frmts/mrsid -DMRSID_ROOT=/usr/local -DCMAKE_PREFIX_PATH=/tmp/install-gdal
cmake --build . "-j$(nproc)"
test -f gdal_MrSID.so
cd ..

# Test building OCI driver in standalone mode
mkdir build_oci
cd build_oci
cmake -S "${GDAL_SOURCE_DIR:=..}/ogr/ogrsf_frmts/oci" "-DOracle_ROOT=$PWD/../instantclient_19_23" -DCMAKE_PREFIX_PATH=/tmp/install-gdal
cmake --build . "-j$(nproc)"
test -f ogr_OCI.so
cd ..

# Test building GeoRaster driver in standalone mode
mkdir build_georaster
cd build_georaster
cmake -S "${GDAL_SOURCE_DIR:=..}/frmts/georaster" -DCMAKE_PREFIX_PATH=/tmp/install-gdal "-DOracle_ROOT=$PWD/../instantclient_19_23"
cmake --build . "-j$(nproc)"
test -f gdal_GEOR.so
cd ..

# Test building Parquet driver in standalone mode
mkdir build_parquet
cd build_parquet
cmake -S "${GDAL_SOURCE_DIR:=..}/ogr/ogrsf_frmts/parquet" -DCMAKE_PREFIX_PATH=/tmp/install-gdal
cmake --build . "-j$(nproc)"
test -f ogr_Parquet.so
cd ..

# Test building Arrow driver in standalone mode
mkdir build_arrow
cd build_arrow
cmake -S "${GDAL_SOURCE_DIR:=..}/ogr/ogrsf_frmts/arrow" -DCMAKE_PREFIX_PATH=/tmp/install-gdal
cmake --build . "-j$(nproc)"
test -f ogr_Arrow.so
cd ..

# Test building OpenJPEG driver in standalone mode
mkdir build_openjpeg
cd build_openjpeg
cmake -S "${GDAL_SOURCE_DIR:=..}/frmts/openjpeg" -DCMAKE_PREFIX_PATH=/tmp/install-gdal
cmake --build . "-j$(nproc)"
test -f gdal_JP2OpenJPEG.so
cd ..

# Test building TileDB driver in standalone mode
mkdir build_tiledb
cd build_tiledb
cmake -S "${GDAL_SOURCE_DIR:=..}/frmts/tiledb" -DCMAKE_PREFIX_PATH=/tmp/install-gdal
cmake --build . "-j$(nproc)"
test -f gdal_TileDB.so
cd ..

# Test building ECW driver in standalone mode
mkdir build_ecw
cd build_ecw
cmake -S "${GDAL_SOURCE_DIR:=..}/frmts/ecw" -DCMAKE_PREFIX_PATH=/tmp/install-gdal -DECW_ROOT=/opt/libecwj2-3.3
cmake --build . "-j$(nproc)"
test -f gdal_ECW_JP2ECW.so
cd ..
