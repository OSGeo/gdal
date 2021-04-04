#!/bin/sh

set -e

sudo apt-get purge -y libgeos*
(sudo mv /etc/apt/sources.list.d/pgdg* /tmp || /bin/true)
#sudo apt-get remove postgresql-9.2
#sudo apt-get remove postgis libpq5 libpq-dev postgresql-9.1-postgis postgresql-9.1-postgis-2.2-scripts postgresql-9.2-postgis postgresql-9.3-postgis postgresql-9.1 postgresql-9.2 postgresql-9.3 libgdal1
sudo add-apt-repository -y ppa:ubuntugis/ubuntugis-unstable
#sudo add-apt-repository -y ppa:marlam/gta
sudo apt-get update -qq
sudo apt-get install python-numpy postgis libpq-dev libpng12-dev libjpeg-dev libgif-dev liblzma-dev libgeos-dev libcurl4-gnutls-dev libproj-dev libxml2-dev libexpat-dev libxerces-c-dev libnetcdf-dev netcdf-bin libpoppler-dev libpoppler-private-dev libspatialite-dev gpsbabel swig libhdf4-alt-dev libhdf5-serial-dev libhdf5-dev libpodofo-dev poppler-utils libfreexl-dev unixodbc-dev libwebp-dev openjdk-8-jdk libepsilon-dev liblcms2-2 libpcre3-dev mercurial cmake libkml-dev libopenjp2-7-dev libzstd1-dev sqlite3
# libgda-dev
# libcrypto++-dev
#sudo apt-get install python-lxml
#sudo apt-get install python-pip
sudo apt-get install libogdi3.2-dev
# Boost for Mongo
#sudo apt-get install libboost-regex-dev libboost-system-dev libboost-thread-dev
#sudo pip install pyflakes
#pyflakes autotest
#pyflakes gdal/swig/python/gdal-utils/scripts
#pyflakes gdal/swig/python/gdal-utils/osgeo_utils/samples
#psql -c "drop database if exists autotest" -U postgres
#psql -c "create database autotest" -U postgres
#psql -c "create extension postgis" -d autotest -U postgres
#mysql -e "create database autotest;"
#mysql -e "GRANT ALL ON autotest.* TO 'root'@'localhost';" -u root
#mysql -e "GRANT ALL ON autotest.* TO 'travis'@'localhost';" -u root

wget https://github.com/Esri/file-geodatabase-api/raw/master/FileGDB_API_1.5/FileGDB_API_1_5_64gcc51.tar.gz
tar xzf FileGDB_API_1_5_64gcc51.tar.gz
sudo cp -r FileGDB_API-64gcc51/include/* /usr/local/include
sudo cp -r FileGDB_API-64gcc51/lib/* /usr/local/lib

wget http://s3.amazonaws.com/etc-data.koordinates.com/gdal-travisci/install-libecwj2-ubuntu12.04-64bit.tar.gz
tar xzf install-libecwj2-ubuntu12.04-64bit.tar.gz
sudo cp -r install-libecwj2/include/* /usr/local/include
sudo cp -r install-libecwj2/lib/* /usr/local/lib

wget https://github.com/ubarsc/kealib/archive/kealib-1.4.12.zip
unzip kealib-1.4.12.zip
(cd kealib-kealib-1.4.12;
cmake . -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DHDF5_INCLUDE_DIR=/usr/include -DHDF5_LIB_PATH=/usr/lib -DLIBKEA_WITH_GDAL=OFF;
make -j4;
sudo make install)
sudo ldconfig

FILE=clang+llvm-9.0.0-x86_64-linux-gnu-ubuntu-16.04.tar.xz
URL_ROOT=https://github.com/rouault/gdal_ci_tools/raw/master/${FILE}
curl -Ls ${URL_ROOT}aa ${URL_ROOT}ab ${URL_ROOT}ac ${URL_ROOT}ad ${URL_ROOT}ae | tar xJf -
