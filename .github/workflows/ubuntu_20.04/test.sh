#!/bin/bash

set -eu

export OGR_HANA_CONNECTION_STRING='DRIVER=/usr/sap/hdbclient/libodbcHDB.so;HOST=917df316-4e01-4a10-be54-eac1b6ab15fb.hana.prod-us10.hanacloud.ondemand.com;PORT=443;USER=GDALCI;PASSWORD=u7t!Ukeugzq7'

# Because of MrSID and ECW that create temporary files in the current directory
export CHECK_NO_UNINTENDED_FILES=NO

ctest -V -j $(nproc)
