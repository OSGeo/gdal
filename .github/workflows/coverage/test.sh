#!/bin/bash

set -eu

export OGR_HANA_CONNECTION_STRING='DRIVER=/usr/sap/hdbclient/libodbcHDB.so;HOST=917df316-4e01-4a10-be54-eac1b6ab15fb.hana.prod-us10.hanacloud.ondemand.com;PORT=443;USER=GDALCI;PASSWORD=u7t!Ukeugzq7'

ctest -V -j $(nproc)

lcov --directory . --capture --output-file gdal.info 2>/dev/null
cp gdal.info gdal_filtered.info
lcov --remove gdal_filtered.info '/usr/*' --output-file gdal_filtered.info
lcov --remove gdal_filtered.info '/opt/*' --output-file gdal_filtered.info
lcov --remove gdal_filtered.info '*jpeg/libjpeg*' --output-file gdal_filtered.info
lcov --remove gdal_filtered.info '*pcraster/libcsf*' --output-file gdal_filtered.info
lcov --remove gdal_filtered.info '*googletest-src*' --output-file gdal_filtered.info
genhtml -o ./coverage_html --ignore-errors source --num-spaces 2 gdal_filtered.info >/dev/null

if test "${COVERALLS_REPO_TOKEN:-}" != ""; then
  pip install cpp-coveralls
  echo "Running git config --global --add safe.directory ${GDAL_SOURCE_DIR}"
  git config --global --add safe.directory ${GDAL_SOURCE_DIR}
  coveralls -n -l gdal_filtered.info
fi
