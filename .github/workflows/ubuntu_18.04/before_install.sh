#!/bin/sh

set -e

sudo apt-get update

# MSSQL: server side
docker pull mcr.microsoft.com/mssql/server:2017-latest
docker run -e 'ACCEPT_EULA=Y' -e 'SA_PASSWORD=DummyPassw0rd'  -p 1433:1433 --name sql1 -d mcr.microsoft.com/mssql/server:2017-latest
sleep 10
docker exec -t sql1 /opt/mssql-tools/bin/sqlcmd -l 30 -S localhost -U SA -P DummyPassw0rd -Q "CREATE DATABASE TestDB;"

# MySQL 8
docker run --name mysql1 -e MYSQL_ROOT_PASSWORD=passwd -e "MYSQL_ROOT_HOST=%" -p 33060:3306 -d mysql/mysql-server:8.0.18 mysqld --default-authentication-plugin=mysql_native_password

# MariaDB 10.3.9
docker run --name mariadb -e MYSQL_ROOT_PASSWORD=passwd -e "MYSQL_ROOT_HOST=%" -p 33061:3306 -d mariadb:10.3.9

# PostGIS
docker run -v /home:/home --name "postgis" -p 25432:5432 -e ALLOW_IP_RANGE=0.0.0.0/0 -d -t kartoza/postgis:13.0

# Azurite (Azure simulator)
docker run -d -p 10000:10000 mcr.microsoft.com/azure-storage/azurite azurite-blob --blobHost 0.0.0.0

sudo apt-get install -y --allow-unauthenticated python3-dev python3-pip python3-numpy libpng-dev libjpeg-dev libgif-dev liblzma-dev libgeos-dev libcurl4-gnutls-dev libproj-dev libxml2-dev libexpat-dev libxerces-c-dev libnetcdf-dev netcdf-bin libpoppler-dev libpoppler-private-dev libspatialite-dev gpsbabel swig libhdf4-alt-dev libhdf5-serial-dev poppler-utils unixodbc-dev libwebp-dev libepsilon-dev liblcms2-2 libpcre3-dev libcrypto++-dev libfyba-dev libkml-dev libmysqlclient-dev mysql-client-core-5.7 libogdi3.2-dev libcfitsio-dev openjdk-8-jdk libzstd1-dev libblosc-dev liblz4-dev ccache bash zip curl libpq-dev postgresql-client postgis cmake libssl-dev libboost-dev autoconf automake sqlite3 libopenexr-dev g++ fossil libgeotiff-dev libopenjp2-7-dev libcairo2-dev doxygen
# libheif-dev: strane linking errors (__cxa_init_primary_exception, std::thread::_State::~_State()) related to also linking to FileGDB API
# libpodofo-dev : FIXME incompatibilities at runtime with that version
#sudo apt-get install -y --allow-unauthenticated libsfcgal-dev

# MSSQL: client side
curl https://packages.microsoft.com/keys/microsoft.asc | sudo apt-key add -
curl https://packages.microsoft.com/config/ubuntu/18.04/prod.list | sudo tee /etc/apt/sources.list.d/msprod.list
sudo apt-get update
sudo ACCEPT_EULA=Y apt-get install -y msodbcsql17 unixodbc-dev

# Initialize MySQL & MariaDB databases
echo 'CREATE DATABASE test' | mysql -uroot -ppasswd --port=33060 -h "$IP"
echo 'CREATE DATABASE test' | mysql -uroot -ppasswd --port=33061 -h "$IP"

# Initialize PostGIS
PGPASSWORD=docker psql -h "$IP" -U docker -p 25432 -d gis -c "CREATE DATABASE autotest"
PGPASSWORD=docker psql -h "$IP" -U docker -p 25432 -d autotest -c "CREATE EXTENSION postgis"
PGPASSWORD=docker psql -h "$IP" -U docker -p 25432 -d autotest -c "CREATE EXTENSION postgis_raster"

# Mongo
docker run --name mongo -p 27018:27017 -d mongo:4.4

wget https://github.com/Esri/file-geodatabase-api/raw/master/FileGDB_API_1.5/FileGDB_API_1_5_64gcc51.tar.gz
tar xzf FileGDB_API_1_5_64gcc51.tar.gz
sudo cp -r FileGDB_API-64gcc51/* /usr/
sudo ldconfig

wget https://github.com/mongodb/mongo-c-driver/releases/download/1.13.0/mongo-c-driver-1.13.0.tar.gz
wget https://github.com/mongodb/mongo-cxx-driver/archive/r3.4.0.tar.gz
