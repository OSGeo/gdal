#!/bin/bash

set -e

. ../scripts/setdevenv.sh

LD_LIBRARY_PATH="/opt/instantclient_19_9:/opt/instantclient_19_9/lib:${LD_LIBRARY_PATH}"
export LD_LIBRARY_PATH

export PYTEST="python3 -m pytest -vv -p no:sugar --color=no"

# Run C++ tests
make quicktest

IP=host.docker.internal

# Test /vsiaz/ against the Azurite simulator
export AZURITE_STORAGE_CONNECTION_STRING="DefaultEndpointsProtocol=http;AccountName=devstoreaccount1;AccountKey=Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsuFq2UVErCz4I6tq/K1SZFPTOtr/KBHBeksoGMGw==;BlobEndpoint=http://${IP}:10000/devstoreaccount1;"
AZURE_STORAGE_CONNECTION_STRING=${AZURITE_STORAGE_CONNECTION_STRING} python3 -c "from osgeo import gdal; import sys; sys.exit(gdal.Mkdir('/vsiaz/mycontainer', 0o755))"
(cd autotest && AZURE_STORAGE_CONNECTION_STRING=${AZURITE_STORAGE_CONNECTION_STRING} AZ_RESOURCE=mycontainer $PYTEST gcore/vsiaz_real_instance_manual.py)

# MySQL 8
(cd autotest && OGR_MYSQL_CONNECTION_STRING=mysql:test,user=root,password=passwd,port=33060,host=$IP $PYTEST ogr/ogr_mysql.py)
# MariaDB 10.3.9
(cd autotest && OGR_MYSQL_CONNECTION_STRING=mysql:test,user=root,password=passwd,port=33061,host=$IP $PYTEST ogr/ogr_mysql.py)

# PostGIS tests
(cd autotest && OGR_PG_CONNECTION_STRING="host=$IP port=25432 dbname=autotest user=docker password=docker" $PYTEST --capture=no -ra ogr/ogr_pg.py)
(cd autotest && PGHOST="$IP" PGPORT=25432 PGUSER=docker PGPASSWORD=docker $PYTEST --capture=no -ra gdrivers/postgisraster.py)

# MongoDB v3
(cd autotest && MONGODBV3_TEST_PORT=27018 MONGODBV3_TEST_HOST=$IP $PYTEST ogr/ogr_mongodbv3.py)

(cd autotest && OGR_MSSQL_CONNECTION_STRING="MSSQL:server=$IP;database=TestDB;driver=ODBC Driver 17 for SQL Server;UID=SA;PWD=DummyPassw0rd" $PYTEST ogr/ogr_mssqlspatial.py)

(cd autotest && $PYTEST)
