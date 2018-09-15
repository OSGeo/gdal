#!/bin/sh

set -e

export chroot="$PWD"/bionic
export LC_ALL=en_US.utf8

chroot "$chroot" sh -c "cd $PWD/autotest/cpp && make quick_test"
# Compile and test vsipreload
chroot "$chroot" sh -c "cd $PWD/autotest/cpp && make vsipreload.so"
# Run all the Python autotests

# Run ogr_fgdb test in isolation due to likely conflict with libxml2
chroot "$chroot" sh -c "cd $PWD/autotest/ogr && python ogr_fgdb.py && cd ../../.."
rm autotest/ogr/ogr_fgdb.py

# MySQL 8
chroot "$chroot" sh -c "cd $PWD/autotest/ogr && OGR_MYSQL_CONNECTION_STRING=mysql:test,user=root,password=passwd,port=33060,host=127.0.0.1 python ogr_mysql.py"
# MariaDB 10.3.9
chroot "$chroot" sh -c "cd $PWD/autotest/ogr && OGR_MYSQL_CONNECTION_STRING=mysql:test,user=root,password=passwd,port=33061,host=127.0.0.1 python ogr_mysql.py"

# PostGIS tests
chroot "$chroot" sh -c "cd $PWD/autotest/ogr && OGR_PG_CONNECTION_STRING='host=127.0.0.1 port=25432 dbname=autotest user=docker password=docker' python ogr_pg.py"
chroot "$chroot" sh -c "cd $PWD/autotest/gdrivers && PGHOST=127.0.0.1 PGPORT=25432 PGUSER=docker PGPASSWORD=docker python postgisraster.py"

# for some reason connection to the DB requires sudo chroot
# WARNING: unfortunately this doesn't even work from the ubuntu 18.04 chroot, but it
# does from the ubuntu 16.04 one
sudo chroot "$chroot" sh -c "cd $PWD/autotest/ogr && python ogr_mssqlspatial.py && cd ../../.."

# Fails with ERROR 1: OGDI DataSource Open Failed: Could not find the dynamic library "vrf"
rm autotest/ogr/ogr_ogdi.py

chroot "$chroot" sh -c "cd $PWD/autotest && python run_all.py"