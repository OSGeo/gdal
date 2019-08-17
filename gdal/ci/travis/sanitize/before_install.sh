#!/bin/sh

set -e

sudo mv /etc/apt/sources.list.d/pgdg* /tmp
sudo apt-get update
sudo apt-get install -y software-properties-common python-software-properties
sudo add-apt-repository -y ppa:ubuntugis/ubuntugis-unstable
sudo apt-get update
# Disable postgresql since it draws ssl-cert that doesn't install cleanly
# postgis postgresql-9.1 postgresql-client-9.1 postgresql-9.1-postgis-2.1 postgresql-9.1-postgis-2.1-scripts libpq-dev
sudo apt-get install -y --allow-unauthenticated python-numpy libpng12-dev libjpeg-dev libgif-dev liblzma-dev libgeos-dev libcurl4-gnutls-dev libproj-dev libxml2-dev libexpat-dev libxerces-c-dev libnetcdf-dev netcdf-bin libpoppler-dev libpoppler-private-dev libsqlite3-dev gpsbabel swig libhdf4-alt-dev libhdf5-serial-dev libpodofo-dev poppler-utils libfreexl-dev unixodbc-dev libwebp-dev libepsilon-dev liblcms2-2 libpcre3-dev libcrypto++-dev libdap-dev libfyba-dev libmysqlclient-dev libogdi3.2-dev libcfitsio-dev openjdk-8-jdk couchdb libzstd1-dev ccache curl autoconf automake sqlite3 libspatialite-dev
sudo apt-get install -y doxygen texlive-latex-base
sudo apt-get install -y make
sudo apt-get install -y python-dev
sudo apt-get install -y g++
sudo apt-get install -y --allow-unauthenticated libsfcgal-dev
sudo apt-get install -y --allow-unauthenticated fossil libgeotiff-dev libcharls-dev libopenjp2-7-dev libcairo2-dev

wget https://github.com/Esri/file-geodatabase-api/raw/master/FileGDB_API_1.5/FileGDB_API_1_5_64gcc51.tar.gz
tar xzf FileGDB_API_1_5_64gcc51.tar.gz
sudo cp FileGDB_API-64gcc51/lib/* /usr/lib
sudo ldconfig
