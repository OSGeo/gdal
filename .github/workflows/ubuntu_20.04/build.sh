#!/bin/sh

set -eu

if test "${COVERITY_SCAN_TOKEN:-}" = ""; then
  if test -f /build/ccache.tar.gz; then
      echo "Restoring ccache..."
      (cd $HOME && tar xzf /build/ccache.tar.gz)
  fi

  ccache -M 200M
  ccache -s

  export CXXFLAGS="-std=c++17 -march=native -O2 -Wodr -flto-odr-type-merging -Werror"
  export CFLAGS="-O2 -march=native -Werror"
  export OTHER_SWITCHES="-DUSE_CCACHE=ON -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON"
else
  wget -q https://scan.coverity.com/download/cxx/linux64 --post-data "token=$COVERITY_SCAN_TOKEN&project=GDAL" -O cov-analysis-linux64.tar.gz
  mkdir /tmp/cov-analysis-linux64
  tar xzf cov-analysis-linux64.tar.gz --strip 1 -C /tmp/cov-analysis-linux64
  export OTHER_SWITCHES="-DCMAKE_BUILD_TYPE=Debug -DBUILD_PYTHON_BINDINGS=OFF -DBUILD_JAVA_BINDINGS=OFF -DBUILD_CSHARP_BINDINGS=OFF"
fi

cd /build

mkdir build
cd build
cmake .. ${OTHER_SWITCHES} \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DGDAL_USE_TIFF_INTERNAL=ON \
    -DGDAL_USE_GEOTIFF_INTERNAL=ON \
    -DECW_ROOT=/opt/libecwj2-3.3 \
    -DMRSID_ROOT=/usr/local \
    -DFileGDB_ROOT=/usr/local/FileGDB_API

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

make install "-j$(nproc)"
cd ..
ldconfig

ccache -s

echo "Saving ccache..."
rm -f /build/ccache.tar.gz
(cd $HOME && tar czf /build/ccache.tar.gz .ccache)
