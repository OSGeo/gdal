#!/bin/sh

set -eu

git config --global --add safe.directory ${GDAL_SOURCE_DIR:=..}

wget -q https://scan.coverity.com/download/cxx/linux64 --post-data "token=$COVERITY_SCAN_TOKEN&project=GDAL" -O cov-analysis-linux64.tar.gz
mkdir /tmp/cov-analysis-linux64
tar xzf cov-analysis-linux64.tar.gz --strip 1 -C /tmp/cov-analysis-linux64

cmake ${GDAL_SOURCE_DIR:=..} \
    -DCMAKE_BUILD_TYPE=Debug \
    -DBUILD_PYTHON_BINDINGS=OFF \
    -DBUILD_JAVA_BINDINGS=OFF \
    -DBUILD_CSHARP_BINDINGS=OFF \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DGDAL_USE_TIFF_INTERNAL=ON \
    -DGDAL_USE_GEOTIFF_INTERNAL=ON \
    -DECW_ROOT=/opt/libecwj2-3.3 \
    -DMRSID_ROOT=/usr/local \
    -DFileGDB_ROOT=/usr/local/FileGDB_API

/tmp/cov-analysis-linux64/bin/cov-build --dir cov-int make "-j$(nproc)"
tar czf cov-int.tgz cov-int
curl \
    --form token=$COVERITY_SCAN_TOKEN \
    --form email=$COVERITY_SCAN_EMAIL \
    --form file=@cov-int.tgz \
    --form version=master \
    --form description="`git rev-parse --abbrev-ref HEAD` `git rev-parse --short HEAD`" \
    https://scan.coverity.com/builds?project=GDAL
