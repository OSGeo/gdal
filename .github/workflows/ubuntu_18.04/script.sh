#!/bin/sh

set -e

export PYTEST="python3 -m pytest -vv -p no:sugar --color=no"

(cd autotest/cpp && make quick_test)
# Compile and test vsipreload
(cd autotest/cpp && make vsipreload.so)

# install pip and use it to install test dependencies
pip3 install -U -r autotest/requirements.txt

# Run all the Python autotests

# Run ogr_fgdb test in isolation due to likely conflict with libxml2
(cd autotest/ogr && $PYTEST ogr_fgdb.py)
rm autotest/ogr/ogr_fgdb.py

# Test /vsiaz/ against the Azurite simulator
export AZURITE_STORAGE_CONNECTION_STRING="DefaultEndpointsProtocol=http;AccountName=devstoreaccount1;AccountKey=Eby8vdM02xNOcqFlqUwJPLlmEtlCDXJ1OUzFT50uSRZ6IFsuFq2UVErCz4I6tq/K1SZFPTOtr/KBHBeksoGMGw==;BlobEndpoint=http://${IP}:10000/devstoreaccount1;"
AZURE_STORAGE_CONNECTION_STRING=${AZURITE_STORAGE_CONNECTION_STRING} python3 -c "from osgeo import gdal; import sys; sys.exit(gdal.Mkdir('/vsiaz/mycontainer', 0o755))"
(cd autotest/gcore && AZURE_STORAGE_CONNECTION_STRING=${AZURITE_STORAGE_CONNECTION_STRING} AZ_RESOURCE=mycontainer $PYTEST vsiaz_real_instance_manual.py)

# MySQL 8
(cd autotest/ogr && OGR_MYSQL_CONNECTION_STRING=mysql:test,user=root,password=passwd,port=33060,host=$IP $PYTEST ogr_mysql.py)
# MariaDB 10.3.9
(cd autotest/ogr && OGR_MYSQL_CONNECTION_STRING=mysql:test,user=root,password=passwd,port=33061,host=$IP $PYTEST ogr_mysql.py)

# PostGIS tests
(cd autotest/ogr && OGR_PG_CONNECTION_STRING="host=$IP port=25432 dbname=autotest user=docker password=docker" $PYTEST --capture=no -ra ogr_pg.py)
(cd autotest/gdrivers && PGHOST="$IP" PGPORT=25432 PGUSER=docker PGPASSWORD=docker $PYTEST --capture=no -ra postgisraster.py)

# MongoDB v3
(cd autotest/ogr && MONGODBV3_TEST_PORT=27018 MONGODBV3_TEST_HOST=$IP $PYTEST ogr_mongodbv3.py)

(cd autotest/ogr && OGR_MSSQL_CONNECTION_STRING="MSSQL:server=$IP;database=TestDB;driver=ODBC Driver 17 for SQL Server;UID=SA;PWD=DummyPassw0rd" $PYTEST ogr_mssqlspatial.py)

# Fails with ERROR 1: OGDI DataSource Open Failed: Could not find the dynamic library "vrf"
rm autotest/ogr/ogr_ogdi.py

(cd autotest && $PYTEST)
