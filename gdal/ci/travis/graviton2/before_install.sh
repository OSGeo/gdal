#!/bin/sh

set -e

sudo apt-get update
sudo apt-get install -y software-properties-common
sudo apt-get update
sudo apt-get install -y --allow-unauthenticated python3-numpy libpng-dev libjpeg-dev libgif-dev liblzma-dev libgeos-dev libcurl4-gnutls-dev libproj-dev libxml2-dev libexpat-dev libxerces-c-dev libnetcdf-dev netcdf-bin libpoppler-dev libpoppler-private-dev libspatialite-dev gpsbabel swig libhdf4-alt-dev libhdf5-serial-dev poppler-utils libfreexl-dev unixodbc-dev libwebp-dev libepsilon-dev liblcms2-2 libpcre3-dev libcrypto++-dev libdap-dev libfyba-dev libkml-dev libmysqlclient-dev mysql-client-core-5.7 libogdi3.2-dev libcfitsio-dev openjdk-8-jdk libzstd1-dev libblosc-dev liblz4-dev ccache bash zip curl libpq-dev postgresql-client postgis cmake libssl-dev libboost-dev autoconf automake sqlite3 libopenexr-dev make python3-dev g++ fossil libgeotiff-dev libcharls-dev libopenjp2-7-dev libcairo2-dev

# Workaround bug in ogdi packaging
sudo ln -s /usr/lib/ogdi/libvrf.so /usr/lib
