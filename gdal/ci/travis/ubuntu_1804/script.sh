#!/bin/sh

set -e

export chroot="$PWD"/bionic
export LC_ALL=en_US.utf8
export PYTEST="pytest -vv -p no:sugar --color=no"

chroot "$chroot" sh -c "cd $PWD/autotest/cpp && make quick_test"
# Compile and test vsipreload
chroot "$chroot" sh -c "cd $PWD/autotest/cpp && make vsipreload.so"

# install pip and use it to install test dependencies
sudo chroot "$chroot" sh -c "curl -sSL 'https://bootstrap.pypa.io/get-pip.py' | python"
sudo chroot "$chroot" pip install -U -r "$PWD/autotest/requirements.txt"

# Run all the Python autotests

# Run ogr_fgdb test in isolation due to likely conflict with libxml2
chroot "$chroot" sh -c "cd $PWD/autotest/ogr && $PYTEST ogr_fgdb.py && cd ../../.."
rm autotest/ogr/ogr_fgdb.py

# MySQL 8
chroot "$chroot" sh -c "cd $PWD/autotest/ogr && OGR_MYSQL_CONNECTION_STRING=mysql:test,user=root,password=passwd,port=33060,host=127.0.0.1 $PYTEST ogr_mysql.py"
# MariaDB 10.3.9
chroot "$chroot" sh -c "cd $PWD/autotest/ogr && OGR_MYSQL_CONNECTION_STRING=mysql:test,user=root,password=passwd,port=33061,host=127.0.0.1 $PYTEST ogr_mysql.py"

# PostGIS tests
chroot "$chroot" sh -c "cd $PWD/autotest/ogr && OGR_PG_CONNECTION_STRING='host=127.0.0.1 port=25432 dbname=autotest user=docker password=docker' $PYTEST ogr_pg.py"
chroot "$chroot" sh -c "cd $PWD/autotest/gdrivers && PGHOST=127.0.0.1 PGPORT=25432 PGUSER=docker PGPASSWORD=docker $PYTEST postgisraster.py"

# MongoDB v3
chroot "$chroot" sh -c "cd $PWD/autotest/ogr && MONGODBV3_TEST_PORT=27018 MONGODBV3_TEST_HOST=localhost $PYTEST ogr_mongodbv3.py"

# for some reason connection to the DB requires sudo chroot
# WARNING: unfortunately this doesn't even work from the ubuntu 18.04 chroot, but it
# does from the ubuntu 16.04 one
sudo chroot "$chroot" sh -c "cd $PWD/autotest/ogr && $PYTEST ogr_mssqlspatial.py && cd ../../.."

# Fails with ERROR 1: OGDI DataSource Open Failed: Could not find the dynamic library "vrf"
rm autotest/ogr/ogr_ogdi.py

chroot "$chroot" sh -c "cd $PWD/autotest && $PYTEST"