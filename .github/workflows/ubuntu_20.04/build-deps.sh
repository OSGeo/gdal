#!/bin/sh

set -eu

apt-get update -y
DEBIAN_FRONTEND=noninteractive apt-get install -y --fix-missing --no-install-recommends \
    software-properties-common build-essential ca-certificates \
    git make cmake wget zip unzip libtool automake \
    zlib1g-dev libsqlite3-dev pkg-config libcurl4-gnutls-dev \
    libproj-dev libtiff5-dev \
    libcharls-dev libopenjp2-7-dev libcairo2-dev \
    python3-dev python3-numpy python3-pip \
    libpng-dev libjpeg-dev libgif-dev liblzma-dev libgeos-dev \
    curl libxml2-dev libexpat-dev libxerces-c-dev \
    libnetcdf-dev libpoppler-dev libpoppler-private-dev \
    libspatialite-dev swig libhdf4-alt-dev libhdf5-serial-dev \
    libfreexl-dev unixodbc-dev libwebp-dev libepsilon-dev \
    liblcms2-2 libpcre2-dev libcrypto++-dev libdap-dev libfyba-dev \
    libkml-dev libmysqlclient-dev libogdi-dev \
    libcfitsio-dev openjdk-8-jdk libzstd-dev \
    libpq-dev libssl-dev libboost-dev \
    autoconf automake bash-completion libarmadillo-dev \
    libopenexr-dev libheif-dev \
    libdeflate-dev libblosc-dev liblz4-dev \
    mono-mcs libmono-system-drawing4.0-cil ccache \
    perl ant \
    libbrotli-dev \
    opencl-c-headers ocl-icd-opencl-dev

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
    && rm -rf tiledeb

# Workaround bug in ogdi packaging
ln -s /usr/lib/ogdi/libvrf.so /usr/lib

# Build libjxl
# libjxl being still unstable, if the main branch fails to compile/test
# you can replace JXL_TREEISH=main by JXL_TREEISH=sha1_of_known_working_commit
JXL_TREEISH=main
# Mention commit ea2612d6df99e9878b51e315935f9d6201f5fe47 to force rebuild of dep
git clone https://github.com/libjxl/libjxl.git --recursive \
    && cd libjxl \
    && git checkout ${JXL_TREEISH} \
    && mkdir build \
    && cd build \
    && cmake -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTING=OFF .. \
    && make -j$(nproc) \
    && make -j$(nproc) install \
    && cd ../.. \
    && rm -rf libjxl

# Install MrSID SDK
mkdir mrsid \
    && wget -q https://bin.extensis.com/download/developer/MrSID_DSDK-9.5.4.4709-rhel6.x86-64.gcc531.tar.gz -O - \
      | tar xz -C mrsid --strip-components=1 \
    && cp -r mrsid/Raster_DSDK/include/* /usr/local/include \
    && cp -r mrsid/Raster_DSDK/lib/* /usr/local/lib \
    && cp -r mrsid/Lidar_DSDK/include/* /usr/local/include \
    && cp -r mrsid/Lidar_DSDK/lib/* /usr/local/lib \
    && sed -i "s/__GNUC__ <= 5/__GNUC__ <= 99/" /usr/local/include/lt_platform.h \
    && cd .. \
    && rm -rf mrsid

# Install ECW SDK
(cd / && wget -q https://github.com/rouault/libecwj2-3.3-builds/releases/download/v1/install-libecwj2-3.3-ubuntu-20.04.tar.gz && tar xzf install-libecwj2-3.3-ubuntu-20.04.tar.gz && rm -f install-libecwj2-3.3-ubuntu-20.04.tar.gz ) \
  && echo "/opt/libecwj2-3.3/lib" > /etc/ld.so.conf.d/libecwj2-3.3.conf

# Install FileGDB API SDK
wget -q https://github.com/Esri/file-geodatabase-api/raw/master/FileGDB_API_1.5.1/FileGDB_API_1_5_1-64gcc51.tar.gz \
  && tar -xzf FileGDB_API_1_5_1-64gcc51.tar.gz --no-same-owner \
  && chown -R root:root FileGDB_API-64gcc51 \
  && mv FileGDB_API-64gcc51 /usr/local/FileGDB_API \
  && rm -rf /usr/local/FileGDB_API/lib/libstdc++* \
  && cp /usr/local/FileGDB_API/include/* /usr/include \
  && rm -rf FileGDB_API_1_5_1-64gcc51.tar.gz \
  && echo "/usr/local/FileGDB_API/lib" > /etc/ld.so.conf.d/filegdbapi.conf

# Build and install GEOS (3.10dev)
GEOS_SHA1=cab7d3cc63dc6ffaa48630b517c9ab69be6505e0
mkdir geos \
    && wget -q https://github.com/libgeos/geos/archive/${GEOS_SHA1}.tar.gz -O - \
        | tar xz -C geos --strip-components=1 \
    && cd geos \
    && mkdir build_cmake \
    && cd build_cmake \
    && cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DBUILD_TESTING=OFF \
    && make -j$(nproc) \
    && make install \
    && cd ../.. \
    && rm -rf geos

# Install pdfium
wget -q https://github.com/rouault/pdfium_build_gdal_3_5/releases/download/v1_pdfium_4933/install-ubuntu2004-rev4933.tar.gz \
  && tar -xzf install-ubuntu2004-rev4933.tar.gz \
  && chown -R root:root install \
  && mv install/lib/* /usr/lib/ \
  && mv install/include/* /usr/include/ \
  && rm -rf install-ubuntu2004-rev4933.tar.gz install \
  && apt-get update -y \
  && apt-get install -y --fix-missing --no-install-recommends liblcms2-dev \
  && rm -rf /var/lib/apt/lists/*

ldconfig
