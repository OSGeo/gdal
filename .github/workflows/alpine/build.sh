#!/bin/sh

set -e

cat << EOF > /tmp/foo.cpp
#include <cstdio>
extern "C" void DeclareDeferredFOO(void);
void DeclareDeferredFOO()
{
    FILE* f = fopen("/tmp/DeclareDeferredFOO_has_been_run.bin", "wb");
    if (f)
      fclose(f);
}
EOF

cmake ${GDAL_SOURCE_DIR:=..} \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_UNITY_BUILD=ON \
  -DUSE_CCACHE=ON \
  -DCMAKE_INSTALL_PREFIX=/usr \
  -DIconv_INCLUDE_DIR=/usr/include/gnu-libiconv \
  -DIconv_LIBRARY=/usr/lib/libiconv.so \
  -DADD_EXTERNAL_DEFERRED_PLUGIN_FOO=/tmp/foo.cpp \
  -DGDAL_ENABLE_PLUGINS=ON \
  -DGDAL_ENABLE_PLUGINS_NO_DEPS=ON \
  -DOGR_ENABLE_DRIVER_TAB_PLUGIN=OFF \
  -DOGR_ENABLE_DRIVER_GEOJSON_PLUGIN=OFF \
  -DCMAKE_CXX_STANDARD=23 \
  -DCMAKE_C_FLAGS=-Werror -DCMAKE_CXX_FLAGS="-Werror" -DWERROR_DEV_FLAG="-Werror=dev"
make -j$(nproc)
make -j$(nproc) install DESTDIR=/tmp/install-gdal

# To check if DeclareDeferredFOO() is called by GDALAllRegister()
apps/gdalinfo --version

if test -f /tmp/DeclareDeferredFOO_has_been_run.bin; then
  echo "DeclareDeferredFOO() has been run"
else
  echo "DeclareDeferredFOO() has NOT been run"
  exit 1
fi

#
echo "Validating gdal --json-usage output"
apps/gdal --json-usage > out.json
export PYTHON_CMD=python3
$PYTHON_CMD -m venv myvenv
source myvenv/bin/activate
$PYTHON_CMD -m pip install -U check-jsonschema
check-jsonschema --schemafile data/gdal_algorithm.schema.json out.json
