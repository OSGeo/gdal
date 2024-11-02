#!/bin/bash

set -eu

export OGR_HANA_CONNECTION_STRING='DRIVER=/usr/sap/hdbclient/libodbcHDB.so;HOST=917df316-4e01-4a10-be54-eac1b6ab15fb.hana.prod-us10.hanacloud.ondemand.com;PORT=443;USER=GDALCI;PASSWORD=u7t!Ukeugzq7'

ctest -V -j $(nproc)

# Run extra tests depending on services
export PYTEST="python3 -m pytest -vv -p no:sugar --color=no"

. ../scripts/setdevenv.sh

IP=host.docker.internal

# MySQL 8
(cd autotest && OGR_MYSQL_CONNECTION_STRING=mysql:test,user=root,password=passwd,port=33060,host=$IP $PYTEST ogr/ogr_mysql.py)
# MariaDB 10.3.9
(cd autotest && OGR_MYSQL_CONNECTION_STRING=mysql:test,user=root,password=passwd,port=33061,host=$IP $PYTEST ogr/ogr_mysql.py)

# PostGIS tests
(cd autotest && OGR_PG_CONNECTION_STRING="host=$IP port=25432 dbname=autotest user=docker password=docker" $PYTEST --capture=no -ra ogr/ogr_pg.py)
(cd autotest && PGHOST="$IP" PGPORT=25432 PGUSER=docker PGPASSWORD=docker $PYTEST --capture=no -ra gdrivers/postgisraster.py)

# Generate coverage report
lcov --directory . --capture --output-file gdal.info 2>/dev/null
cp gdal.info gdal_filtered.info
lcov --remove gdal_filtered.info '/usr/*' --output-file gdal_filtered.info
lcov --remove gdal_filtered.info '/opt/*' --output-file gdal_filtered.info
lcov --remove gdal_filtered.info '*jpeg/libjpeg*' --output-file gdal_filtered.info
lcov --remove gdal_filtered.info '*pcraster/libcsf*' --output-file gdal_filtered.info
lcov --remove gdal_filtered.info '*googletest-src*' --output-file gdal_filtered.info
genhtml -o ./coverage_html --ignore-errors source --num-spaces 2 gdal_filtered.info >/dev/null
