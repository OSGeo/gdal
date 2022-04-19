#!/bin/sh

set -e

export PYTEST="pytest -vv -p no:sugar --color=no"

(cd autotest/cpp && make quick_test)
# Compile and test vsipreload
(cd autotest/cpp && make vsipreload.so)

# install pip and use it to install test dependencies
sudo sh -c "curl -sSL 'https://bootstrap.pypa.io/get-pip.py' | python"
pip install -U -r autotest/requirements.txt

# Run all the Python autotests

# Run ogr_fgdb test in isolation due to likely conflict with libxml2
(cd autotest/ogr && $PYTEST ogr_fgdb.py)
rm autotest/ogr/ogr_fgdb.py

# MySQL 8
(cd autotest/ogr && OGR_MYSQL_CONNECTION_STRING=mysql:test,user=root,password=passwd,port=33060,host=127.0.0.1 $PYTEST ogr_mysql.py)
# MariaDB 10.3.9
(cd autotest/ogr && OGR_MYSQL_CONNECTION_STRING=mysql:test,user=root,password=passwd,port=33061,host=127.0.0.1 $PYTEST ogr_mysql.py)

# PostGIS tests
(cd autotest/ogr && OGR_PG_CONNECTION_STRING='host=127.0.0.1 port=25432 dbname=autotest user=docker password=docker' $PYTEST --capture=no -ra ogr_pg.py)
(cd autotest/gdrivers && PGHOST=127.0.0.1 PGPORT=25432 PGUSER=docker PGPASSWORD=docker $PYTEST --capture=no -ra postgisraster.py)

# MongoDB v3
(cd autotest/ogr && MONGODBV3_TEST_PORT=27018 MONGODBV3_TEST_HOST=localhost $PYTEST ogr_mongodbv3.py)

(cd autotest/ogr && $PYTEST ogr_mssqlspatial.py)

# HANA tests
(cd autotest/ogr && OGR_HANA_CONNECTION_STRING="DRIVER={\'/usr/sap/hdbclient/libodbcHDB.so\'};HOST=917df316-4e01-4a10-be54-eac1b6ab15fb.hana.prod-us10.hanacloud.ondemand.com;PORT=443;USER=QGISCI;PASSWORD=tQ&7W3Klr9!p" $PYTEST ogr_hana.py)

# Fails with ERROR 1: OGDI DataSource Open Failed: Could not find the dynamic library "vrf"
rm autotest/ogr/ogr_ogdi.py

(cd autotest && $PYTEST)
