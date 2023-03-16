#!/bin/sh

set -ex

##################
# Start services #
##################

# MSSQL: server side
docker rm -f gdal-sql1
docker pull mcr.microsoft.com/mssql/server:2017-latest
docker run  -e 'ACCEPT_EULA=Y' -e 'SA_PASSWORD=DummyPassw0rd'  -p 1433:1433 --name gdal-sql1 -d mcr.microsoft.com/mssql/server:2017-latest

# MySQL 8
docker rm -f gdal-mysql1
docker run --name gdal-mysql1 -e MYSQL_ROOT_PASSWORD=passwd -e "MYSQL_ROOT_HOST=%" -p 33060:3306 -d mysql/mysql-server:8.0.18 mysqld --default-authentication-plugin=mysql_native_password

# MariaDB 10.3.9
docker rm -f gdal-mariadb
docker run --name gdal-mariadb -e MYSQL_ROOT_PASSWORD=passwd -e "MYSQL_ROOT_HOST=%" -p 33061:3306 -d mariadb:10.3.9

# PostGIS
docker rm -f gdal-postgis
docker run -v /home:/home --name "gdal-postgis" -p 25432:5432 -e ALLOW_IP_RANGE=0.0.0.0/0 -d -t kartoza/postgis:13.0

# Azurite (Azure simulator)
docker rm -f gdal-azurite
docker run -d -p 10000:10000 --name gdal-azurite mcr.microsoft.com/azure-storage/azurite azurite-blob --blobHost 0.0.0.0

# Mongo
docker rm -f gdal-mongo
docker run --name gdal-mongo -p 27018:27017 -d mongo:4.4

######################
# Configure services #
######################

sleep 10

# MSSQL
docker exec -t gdal-sql1 /opt/mssql-tools/bin/sqlcmd -l 30 -S localhost -U SA -P DummyPassw0rd -Q "CREATE DATABASE TestDB"

# MySQL
docker exec gdal-mysql1 sh -c "echo 'CREATE DATABASE test; SELECT Version()' | mysql -uroot -ppasswd"

# MariaDB
docker exec gdal-mariadb sh -c "echo 'CREATE DATABASE test; SELECT Version()' | mysql -uroot -ppasswd"

# PostGIS
docker exec -e PGPASSWORD=docker gdal-postgis psql -h localhost -U docker -d gis -c "CREATE DATABASE autotest"
docker exec -e PGPASSWORD=docker gdal-postgis psql -h localhost -U docker -d autotest -c "CREATE EXTENSION postgis; CREATE EXTENSION postgis_raster; SELECT postgis_full_version()"

