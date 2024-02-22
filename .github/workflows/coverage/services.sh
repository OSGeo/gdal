#!/bin/sh

set -ex

##################
# Start services #
##################

# MySQL 8
docker rm -f gdal-mysql1
docker run --name gdal-mysql1 -e MYSQL_ROOT_PASSWORD=passwd -e "MYSQL_ROOT_HOST=%" -p 33060:3306 -d mysql/mysql-server:8.0.18 mysqld --default-authentication-plugin=mysql_native_password

# MariaDB 10.3.9
docker rm -f gdal-mariadb
docker run --name gdal-mariadb -e MYSQL_ROOT_PASSWORD=passwd -e "MYSQL_ROOT_HOST=%" -p 33061:3306 -d mariadb:10.3.9

# PostGIS
docker rm -f gdal-postgis
docker run -v /home:/home --name "gdal-postgis" -p 25432:5432 -e ALLOW_IP_RANGE=0.0.0.0/0 -d -t kartoza/postgis:13.0

######################
# Configure services #
######################

sleep 10

# MySQL
docker exec gdal-mysql1 sh -c "echo 'CREATE DATABASE test; SELECT Version()' | mysql -uroot -ppasswd"

# MariaDB
docker exec gdal-mariadb sh -c "echo 'CREATE DATABASE test; SELECT Version()' | mysql -uroot -ppasswd"

# PostGIS
docker exec -e PGPASSWORD=docker gdal-postgis psql -h localhost -U docker -d gis -c "CREATE DATABASE autotest"
docker exec -e PGPASSWORD=docker gdal-postgis psql -h localhost -U docker -d autotest -c "CREATE EXTENSION postgis; CREATE EXTENSION postgis_raster; SELECT postgis_full_version()"

