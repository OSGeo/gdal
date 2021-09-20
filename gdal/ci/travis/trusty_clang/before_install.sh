#!/bin/sh

set -e

sudo dpkg -l | grep geos
sudo apt-get purge -y libgeos* libspatialite*
sudo apt-get remove libpq* postgresql*
find  /etc/apt/sources.list.d
sudo mv /etc/apt/sources.list.d/pgdg* /tmp
sudo add-apt-repository -y ppa:ubuntugis/ppa
sudo add-apt-repository -y ppa:ubuntugis/ubuntugis-testing
#sudo add-apt-repository -y ppa:marlam/gta
sudo apt-get update

# install test dependencies
sudo apt-get install python3.5-dev
curl -sSL 'https://bootstrap.pypa.io/get-pip.py' | sudo python3.5
(cd autotest; sudo -H pip install -U -r ./requirements.txt)

sudo pip install lxml flake8 numpy

# MSSQL: server side
docker pull mcr.microsoft.com/mssql/server:2017-latest
sudo docker run -e 'ACCEPT_EULA=Y' -e 'SA_PASSWORD=DummyPassw0rd'  -p 1433:1433 --name sql1 -d mcr.microsoft.com/mssql/server:2017-latest
sleep 10
docker exec -it sql1 /opt/mssql-tools/bin/sqlcmd -l 30 -S localhost -U SA -P DummyPassw0rd -Q "CREATE DATABASE TestDB;"

sudo apt-get install -y --allow-unauthenticated automake ccache libpng12-dev libjpeg-dev libgif-dev liblzma-dev libgeos-dev libcurl4-gnutls-dev libproj-dev libxml2-dev libexpat-dev libxerces-c-dev libnetcdf-dev netcdf-bin libpoppler-dev libspatialite-dev gpsbabel swig libhdf4-alt-dev libhdf5-serial-dev libpodofo-dev poppler-utils libfreexl-dev unixodbc-dev libwebp-dev  libepsilon-dev  liblcms2-2 libpcre3-dev mercurial cmake libcrypto++-dev postgresql-9.3-postgis-2.2 postgresql-9.3-postgis-scripts libpq-dev
# libgta-dev
sudo apt-get install -y libqhull-dev
sudo apt-get install -y libogdi3.2-dev
# Boost for Mongo
#sudo apt-get install -y libboost-regex-dev libboost-system-dev libboost-thread-dev

sudo apt-get install doxygen texlive-latex-base
# flake8 codes to just emulate pyflakes (http://flake8.pycqa.org/en/latest/user/error-codes.html)
FLAKE8="flake8 --select=F401,F402,F403,F404,F405,F406,F407,F601,F602,F621,F622,F631,F632,F633,F701,F702,F703,F704,F705,F706,F707,F721,F722,F811,F812,F821,F822,F823,F831,F841,F901"
$FLAKE8 autotest
$FLAKE8 gdal/swig/python/gdal-utils/scripts
$FLAKE8 gdal/swig/python/gdal-utils/osgeo_utils/samples
psql -c "drop database if exists autotest" -U postgres
psql -c "create database autotest" -U postgres
psql -c "create extension postgis" -d autotest -U postgres
#mysql -e "create database autotest;"
#mysql -e "GRANT ALL ON autotest.* TO 'root'@'localhost';" -u root
#mysql -e "GRANT ALL ON autotest.* TO 'travis'@'localhost';" -u root
wget http://s3.amazonaws.com/etc-data.koordinates.com/gdal-travisci/FileGDB_API_1_2-64.tar.gz
wget http://s3.amazonaws.com/etc-data.koordinates.com/gdal-travisci/MrSID_DSDK-8.5.0.3422-linux.x86-64.gcc44.tar.gz
wget http://s3.amazonaws.com/etc-data.koordinates.com/gdal-travisci/install-libecwj2-ubuntu12.04-64bit.tar.gz
wget http://s3.amazonaws.com/etc-data.koordinates.com/gdal-travisci/install-libkml-r864-64bit.tar.gz
wget https://github.com/uclouvain/openjpeg/releases/download/v2.3.0/openjpeg-v2.3.0-linux-x86_64.tar.gz
#wget http://even.rouault.free.fr/mongo-cxx-1.0.2-install-ubuntu12.04-64bit.tar.gz
tar xzf MrSID_DSDK-8.5.0.3422-linux.x86-64.gcc44.tar.gz
sudo cp -r MrSID_DSDK-8.5.0.3422-linux.x86-64.gcc44/Raster_DSDK/include/* /usr/local/include
sudo cp -r MrSID_DSDK-8.5.0.3422-linux.x86-64.gcc44/Raster_DSDK/lib/* /usr/local/lib
sudo cp -r MrSID_DSDK-8.5.0.3422-linux.x86-64.gcc44/Lidar_DSDK/include/* /usr/local/include
sudo cp -r MrSID_DSDK-8.5.0.3422-linux.x86-64.gcc44/Lidar_DSDK/lib/* /usr/local/lib
tar xzf FileGDB_API_1_2-64.tar.gz
sudo cp -r FileGDB_API/include/* /usr/local/include
sudo cp -r FileGDB_API/lib/* /usr/local/lib
tar xzf install-libecwj2-ubuntu12.04-64bit.tar.gz
sudo cp -r install-libecwj2/include/* /usr/local/include
sudo cp -r install-libecwj2/lib/* /usr/local/lib
tar xzf install-libkml-r864-64bit.tar.gz
sudo cp -r install-libkml/include/* /usr/local/include
sudo cp -r install-libkml/lib/* /usr/local/lib
tar xzf openjpeg-v2.3.0-linux-x86_64.tar.gz
sudo cp -r openjpeg-v2.3.0-linux-x86_64/include/* /usr/local/include
sudo cp -r openjpeg-v2.3.0-linux-x86_64/lib/* /usr/local/lib
#tar xzf mongo-cxx-1.0.2-install-ubuntu12.04-64bit.tar.gz
#sudo cp -r mongo-cxx-1.0.2-install/include/* /usr/local/include
#sudo cp -r mongo-cxx-1.0.2-install/lib/* /usr/local/lib

wget https://github.com/ubarsc/kealib/archive/kealib-1.4.12.zip
unzip kealib-1.4.12.zip
(cd kealib-kealib-1.4.12;
cmake . -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DHDF5_INCLUDE_DIR=/usr/include -DHDF5_LIB_PATH=/usr/lib -DLIBKEA_WITH_GDAL=OFF;
make -j4;
sudo make install)

# Build zstd
wget https://github.com/facebook/zstd/archive/v1.3.3.tar.gz
tar xvzf v1.3.3.tar.gz
cd zstd-1.3.3/lib
# Faster build
make -j3 PREFIX=/usr ZSTD_LEGACY_SUPPORT=0 CFLAGS=-O1
sudo make install PREFIX=/usr ZSTD_LEGACY_SUPPORT=0 CFLAGS=-O1
cd ../..

# MSSQL: client side
# Disabled because of 'Failed to fetch https://packages.microsoft.com/ubuntu/14.04/prod/dists/trusty/main/binary-amd64/Packages.gz  Hash Sum mismatch'
#sudo bash -c "curl https://packages.microsoft.com/keys/microsoft.asc | apt-key add -"
#sudo bash -c "curl https://packages.microsoft.com/config/ubuntu/14.04/prod.list | tee /etc/apt/sources.list.d/msprod.list"
#sudo apt-get update
#sudo ACCEPT_EULA=Y apt-get install -y msodbcsql17

# Nasty: force reinstallation of unixodbc-dev since the previous line installed unixodbc 2.3.1 from microsoft repo, which lacks the -dev package
# DON'T DO THAT ON YOUR PRODUCTION SERVER.
wget http://mirrors.edge.kernel.org/ubuntu/pool/main/u/unixodbc/libodbc1_2.3.1-4.1_amd64.deb
wget http://mirrors.edge.kernel.org/ubuntu/pool/main/u/unixodbc/odbcinst1debian2_2.3.1-4.1_amd64.deb
wget http://mirrors.edge.kernel.org/ubuntu/pool/main/u/unixodbc/odbcinst_2.3.1-4.1_amd64.deb
wget http://mirrors.edge.kernel.org/ubuntu/pool/main/u/unixodbc/unixodbc-dev_2.3.1-4.1_amd64.deb
wget http://mirrors.edge.kernel.org/ubuntu/pool/main/u/unixodbc/unixodbc_2.3.1-4.1_amd64.deb
sudo dpkg -i --force-all libodbc1_2.3.1-4.1_amd64.deb odbcinst1debian2_2.3.1-4.1_amd64.deb odbcinst_2.3.1-4.1_amd64.deb unixodbc-dev_2.3.1-4.1_amd64.deb unixodbc_2.3.1-4.1_amd64.deb

sudo ldconfig
