#!/bin/bash

set -eu

export OGR_HANA_CONNECTION_STRING='DRIVER=/usr/sap/hdbclient/libodbcHDB.so;HOST=917df316-4e01-4a10-be54-eac1b6ab15fb.hana.prod-us10.hanacloud.ondemand.com;PORT=443;USER=GDALCI;PASSWORD=u7t!Ukeugzq7'

# run all tests except python tests (prefixed with "autotest")
ctest -V -j $(nproc) -E autotest

# run python tests in random order / parallel where possible
. ../scripts/setdevenv.sh
cd autotest
pytest --timeout 120 gnm pyscripts/gdal2tiles
pytest --timeout 120 --random-order alg gcore gdrivers ogr
pytest --timeout 120 --random-order -n $(nproc) pyscripts utilities
