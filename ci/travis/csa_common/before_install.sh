#!/bin/sh

set -e

sudo apt-get update -qq
sudo DEBIAN_FRONTEND=noninteractive apt-get install -y --allow-unauthenticated autoconf \
    automake \
    bash \
    ccache \
    cmake \
    curl \
    doxygen \
    fossil \
    g++ \
    git \
    gpsbabel \
    libblosc-dev \
    libboost-dev \
    libcairo2-dev \
    libcfitsio-dev \
    libcrypto++-dev \
    libcurl4-gnutls-dev \
    libexpat-dev \
    libfcgi-dev \
    libfyba-dev \
    libfreexl-dev \
    libgeos-dev \
    libgeotiff-dev \
    libgif-dev \
    libhdf4-alt-dev \
    libhdf5-serial-dev \
    libjpeg-dev \
    libkml-dev \
    liblcms2-2 \
    liblz4-dev \
    liblzma-dev \
    libmysqlclient-dev \
    libnetcdf-dev \
    libogdi-dev \
    libopenexr-dev \
    libopenjp2-7-dev \
    libpcre3-dev \
    libpng-dev \
    libpoppler-dev \
    libpoppler-private-dev \
    libpq-dev \
    libproj-dev \
    librasterlite2-dev \
    libspatialite-dev \
    libssl-dev \
    libwebp-dev \
    libxerces-c-dev \
    libxml2-dev \
    libxslt-dev \
    libzstd-dev \
    locales \
    mysql-client-core-8.0 \
    netcdf-bin \
    openjdk-8-jdk \
    poppler-utils \
    postgis \
    postgresql-client \
    python3-dev \
    python3-numpy \
    python3-pip \
    sqlite3 \
    swig \
    unixodbc-dev \
    wget \
    zip

sudo DEBIAN_FRONTEND=noninteractive apt-get install -y --allow-unauthenticated clang-tools

wget https://github.com/Esri/file-geodatabase-api/raw/master/FileGDB_API_1.5/FileGDB_API_1_5_64gcc51.tar.gz
tar xzf FileGDB_API_1_5_64gcc51.tar.gz
sudo cp -r FileGDB_API-64gcc51/include/* /usr/local/include
sudo cp -r FileGDB_API-64gcc51/lib/* /usr/local/lib
sudo rm -f /usr/local/lib/libstdc++.so*

wget https://github.com/rouault/libecwj2-3.3-builds/releases/download/v1/install-libecwj2-3.3-ubuntu-20.04.tar.gz
tar xzf install-libecwj2-3.3-ubuntu-20.04.tar.gz
sudo mv opt/libecwj2-3.3 /opt
sudo sh -c 'echo "/opt/libecwj2-3.3/lib" > /etc/ld.so.conf.d/libecwj2-3.3.conf'

wget https://github.com/ubarsc/kealib/archive/kealib-1.4.12.zip
unzip kealib-1.4.12.zip
(cd kealib-kealib-1.4.12;
cmake . -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DHDF5_INCLUDE_DIR=/usr/include -DHDF5_LIB_PATH=/usr/lib -DLIBKEA_WITH_GDAL=OFF;
make -j4;
sudo make install)
sudo ldconfig
