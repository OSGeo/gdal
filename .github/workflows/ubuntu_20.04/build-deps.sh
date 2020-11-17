#!/bin/sh

set -eu

apt-get update -y
DEBIAN_FRONTEND=noninteractive apt-get install -y --fix-missing --no-install-recommends \
    software-properties-common build-essential ca-certificates \
    git make cmake wget unzip libtool automake \
    zlib1g-dev libsqlite3-dev pkg-config libcurl4-gnutls-dev \
    libproj-dev libtiff5-dev \
    libcharls-dev libopenjp2-7-dev libcairo2-dev \
    python3-dev python3-numpy python3-pip \
    libpng-dev libjpeg-dev libgif-dev liblzma-dev libgeos-dev \
    curl libxml2-dev libexpat-dev libxerces-c-dev \
    libnetcdf-dev libpoppler-dev libpoppler-private-dev \
    libspatialite-dev swig libhdf4-alt-dev libhdf5-serial-dev \
    libfreexl-dev unixodbc-dev libwebp-dev libepsilon-dev \
    liblcms2-2 libpcre3-dev libcrypto++-dev libdap-dev libfyba-dev \
    libkml-dev libmysqlclient-dev libogdi-dev \
    libcfitsio-dev openjdk-8-jdk libzstd-dev \
    libpq-dev libssl-dev libboost-dev \
    autoconf automake bash-completion libarmadillo-dev \
    libopenexr-dev libheif-dev \
    libdeflate-dev \
    mono-mcs libmono-system-drawing4.0-cil ccache

# Build likbkea
KEA_VERSION=1.4.13
wget -q https://github.com/ubarsc/kealib/archive/kealib-${KEA_VERSION}.zip \
    && unzip -q kealib-${KEA_VERSION}.zip \
    && rm -f kealib-${KEA_VERSION}.zip \
    && cd kealib-kealib-${KEA_VERSION} \
    && cmake . -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr -DHDF5_INCLUDE_DIR=/usr/include/hdf5/serial \
        -DHDF5_LIB_PATH=/usr/lib/x86_64-linux-gnu/hdf5/serial -DLIBKEA_WITH_GDAL=OFF \
    && make -j$(nproc) \
    && make install \
    && cd .. \
    && rm -rf kealib-kealib-${KEA_VERSION}

# Build tiledb
TILEDB_VERSION=2.0.2
mkdir tiledb \
    && wget -q https://github.com/TileDB-Inc/TileDB/archive/${TILEDB_VERSION}.tar.gz -O - \
        | tar xz -C tiledb --strip-components=1 \
    && cd tiledb \
    && mkdir build_cmake \
    && cd build_cmake \
    && ../bootstrap --prefix=/usr \
    && make -j$(nproc) \
    && make install-tiledb \
    && cd ../.. \
    && rm -f tiledeb

# Workaround bug in ogdi packaging
ln -s /usr/lib/ogdi/libvrf.so /usr/lib
