##
# osgeo/gdal:ubuntu-full

# This file is available at the option of the licensee under:
# Public domain
# or licensed under X/MIT (LICENSE.TXT) Copyright 2019 Even Rouault <even.rouault@spatialys.com>

FROM ubuntu:18.04 as builder

# Derived from osgeo/proj by Howard Butler <howard@hobu.co>
MAINTAINER Even Rouault <even.rouault@spatialys.com>

# Setup build env for PROJ
RUN apt-get update -y \
    && apt-get install -y --fix-missing --no-install-recommends \
            software-properties-common build-essential ca-certificates \
            git make cmake wget unzip libtool automake \
            zlib1g-dev libsqlite3-dev pkg-config sqlite3

# Setup build env for GDAL
RUN apt-get update -y \
    && apt-get install -y --fix-missing --no-install-recommends \
       libcharls-dev libopenjp2-7-dev libcairo2-dev \
       python-dev python-numpy \
       libpng-dev libjpeg-dev libgif-dev liblzma-dev libgeos-dev \
       curl libcurl4-gnutls-dev libxml2-dev libexpat-dev libxerces-c-dev \
       libnetcdf-dev libpoppler-dev libpoppler-private-dev \
       libspatialite-dev swig libhdf4-alt-dev libhdf5-serial-dev \
       libfreexl-dev unixodbc-dev libwebp-dev libepsilon-dev \
       liblcms2-2 libpcre3-dev libcrypto++-dev libdap-dev libfyba-dev \
       libkml-dev libmysqlclient-dev libogdi3.2-dev \
       libcfitsio-dev openjdk-8-jdk libzstd1-dev \
       libpq-dev libssl-dev libboost-dev \
       autoconf automake bash-completion libarmadillo-dev

# Build likbkea
RUN wget -q https://bitbucket.org/chchrsc/kealib/get/c6d36f3db5e4.zip \
    && unzip -q c6d36f3db5e4.zip \
    && rm -f c6d36f3db5e4.zip \
    && cd chchrsc-kealib-c6d36f3db5e4/trunk \
    && cmake . -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr -DHDF5_INCLUDE_DIR=/usr/include/hdf5/serial \
        -DHDF5_LIB_PATH=/usr/lib/x86_64-linux-gnu/hdf5/serial -DLIBKEA_WITH_GDAL=OFF \
    && make -j$(nproc) \
    && make install DESTDIR="/build_thirdparty" \
    && make install \
    && cd ../.. \
    && rm -rf chchrsc-kealib-c6d36f3db5e4 \
    && for i in /build_thirdparty/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && for i in /build_thirdparty/usr/bin/*; do strip -s $i 2>/dev/null || /bin/true; done

# Build mongo-c-driver
RUN wget -q https://github.com/mongodb/mongo-c-driver/releases/download/1.13.0/mongo-c-driver-1.13.0.tar.gz \
    && tar xzf mongo-c-driver-1.13.0.tar.gz \
    && rm -f xzf mongo-c-driver-1.13.0.tar.gz \
    && cd mongo-c-driver-1.13.0 \
    && mkdir build_cmake \
    && cd build_cmake \
    && cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DENABLE_TESTS=NO -DCMAKE_BUILD_TYPE=Release \
    && make -j$(nproc) \
    && make install DESTDIR="/build_thirdparty" \
    && make install \
    && cd ../.. \
    && rm -rf mongo-c-driver-1.13.0 \
    && for i in /build_thirdparty/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && for i in /build_thirdparty/usr/bin/*; do strip -s $i 2>/dev/null || /bin/true; done

# Build mongocxx
RUN wget -q https://github.com/mongodb/mongo-cxx-driver/archive/r3.4.0.tar.gz \
    && tar xzf r3.4.0.tar.gz \
    && rm -f r3.4.0.tar.gz \
    && cd mongo-cxx-driver-r3.4.0 \
    && mkdir build_cmake \
    && cd build_cmake \
    && cmake .. -DCMAKE_INSTALL_PREFIX=/usr -DBSONCXX_POLY_USE_BOOST=ON -DMONGOCXX_ENABLE_SLOW_TESTS=NO -DCMAKE_BUILD_TYPE=Release \
    && make -j$(nproc) \
    && make install DESTDIR="/build_thirdparty" \
    && make install \
    && cd ../.. \
    && rm -rf mongo-cxx-driver-r3.4.0 \
    && for i in /build_thirdparty/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && for i in /build_thirdparty/usr/bin/*; do strip -s $i 2>/dev/null || /bin/true; done

# Build tiledb
RUN wget -q https://github.com/TileDB-Inc/TileDB/archive/1.5.0.tar.gz -O TileDB-1.5.0.tar.gz \
    && tar xzf TileDB-1.5.0.tar.gz \
    && rm -f TileDB-1.5.0.tar.gz \
    && cd TileDB-1.5.0 \
    && mkdir build_cmake \
    && cd build_cmake \
    && ../bootstrap --prefix=/usr \
    && make -j$(nproc) \
    && make install-tiledb DESTDIR="/build_thirdparty" \
    && make install-tiledb \
    && cd ../.. \
    && rm -rf TileDB-1.5.0 \
    && for i in /build_thirdparty/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && for i in /build_thirdparty/usr/bin/*; do strip -s $i 2>/dev/null || /bin/true; done

ARG PROJ_VERSION=master

# Build PROJ
RUN mkdir proj \
    && wget -q https://github.com/OSGeo/proj.4/archive/${PROJ_VERSION}.tar.gz -O - \
        | tar xz -C proj --strip-components=1 \
    && cd proj \
    && ./autogen.sh \
    && CFLAGS='-DPROJ_RENAME_SYMBOLS -O2' CXXFLAGS='-DPROJ_RENAME_SYMBOLS -O2' \
        ./configure --prefix=/usr/local --disable-static \
    && make -j$(nproc) \
    && make install DESTDIR="/build" \
    && cd .. \
    && rm -rf proj \
    && mv /build/usr/local/lib/libproj.so.15.0.0 /build/usr/local/lib/libinternalproj.so.15.0.0 \
    && ln -s libinternalproj.so.15.0.0 /build/usr/local/lib/libinternalproj.so.15 \
    && ln -s libinternalproj.so.15.0.0 /build/usr/local/lib/libinternalproj.so \
    && rm /build/usr/local/lib/libproj.*  \
    && ln -s libinternalproj.so.15.0.0 /build/usr/local/lib/libproj.so.15 \
    && strip -s /build/usr/local/lib/libinternalproj.so.15.0.0 \
    && for i in /build/usr/local/bin/*; do strip -s $i 2>/dev/null || /bin/true; done

ARG GDAL_VERSION=master
ARG GDAL_RELEASE_DATE
ARG GDAL_BUILD_IS_RELEASE

# Build GDAL
RUN if test "${GDAL_VERSION}" = "master"; then \
        export GDAL_VERSION=$(curl -Ls https://api.github.com/repos/OSGeo/gdal/commits/HEAD -H "Accept: application/vnd.github.VERSION.sha"); \
        export GDAL_RELEASE_DATE=$(date "+%Y%m%d"); \
    fi \
    && if test "x${GDAL_BUILD_IS_RELEASE}" = "x"; then \
        export GDAL_SHA1SUM=${GDAL_VERSION}; \
    fi \
    && mkdir gdal \
    && wget -q https://github.com/OSGeo/gdal/archive/${GDAL_VERSION}.tar.gz -O - \
        | tar xz -C gdal --strip-components=1 \
    && cd gdal/gdal \
    && ./configure --prefix=/usr --without-libtool \
    --with-hide-internal-symbols \
    --with-jpeg12 \
    --with-python --with-poppler --with-spatialite --with-mysql --with-liblzma \
    --with-webp --with-epsilon --with-proj=/build/usr/local --with-poppler \
    --with-hdf5 --with-dods-root=/usr --with-sosi --with-mysql \
    --with-libtiff=internal --with-rename-internal-libtiff-symbols \
    --with-geotiff=internal --with-rename-internal-libgeotiff-symbols \
    --with-kea=/usr/bin/kea-config --with-mongocxxv3 --with-tiledb \
    --with-crypto \
    && make -j$(nproc) \
    && make install DESTDIR="/build" \
    && cd ../.. \
    && rm -rf gdal \
    && for i in /build/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && for i in /build/usr/bin/*; do strip -s $i 2>/dev/null || /bin/true; done

# Build final image
FROM ubuntu:18.04 as runner

RUN date

# PROJ dependencies
RUN apt-get update; \
    DEBIAN_FRONTEND=noninteractive apt-get install -y \
        libsqlite3-0 \
        curl unzip

# GDAL dependencies
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y \
        libcharls1 libopenjp2-7 libcairo2 python-numpy \
        libpng16-16 libjpeg-turbo8 libgif7 liblzma5 libgeos-3.6.2 libgeos-c1v5 \
        libcurl4 libxml2 libexpat1 \
        libxerces-c3.2 libnetcdf-c++4 netcdf-bin libpoppler73 libspatialite7 gpsbabel \
        libhdf4-0-alt libhdf5-100 libhdf5-cpp-100 poppler-utils libfreexl1 unixodbc libwebp6 \
        libepsilon1 liblcms2-2 libpcre3 libcrypto++ libdap25 libdapclient6v5 libfyba0 \
        libkmlbase1 libkmlconvenience1 libkmldom1 libkmlengine1 libkmlregionator1 libkmlxsd1 \
        libmysqlclient20 libogdi3.2 libcfitsio5 openjdk-8-jre \
        libzstd1 bash bash-completion libpq5 libssl1.1 \
        libarmadillo8

# Order layers starting with less frequently varying ones
RUN \
    mkdir -p /usr/local/share/proj \
    && curl -LOs http://download.osgeo.org/proj/proj-datumgrid-latest.zip \
    && unzip -j -u -o proj-datumgrid-latest.zip  -d /usr/local/share/proj \
    && curl -LOs http://download.osgeo.org/proj/proj-datumgrid-europe-latest.zip \
    && unzip -j -u -o proj-datumgrid-europe-latest.zip -d /usr/local/share/proj \
    && curl -LOs http://download.osgeo.org/proj/proj-datumgrid-oceania-latest.zip \
    && unzip -j -u -o proj-datumgrid-oceania-latest.zip -d /usr/local/share/proj \
    && curl -LOs http://download.osgeo.org/proj/proj-datumgrid-world-latest.zip  \
    && unzip -j -u -o proj-datumgrid-world-latest.zip -d /usr/local/share/proj \
    && curl -LOs http://download.osgeo.org/proj/proj-datumgrid-north-america-latest.zip \
    && unzip -j -u -o proj-datumgrid-north-america-latest.zip -d /usr/local/share/proj  \
    && rm -f *.zip

COPY --from=builder  /build_thirdparty/usr/ /usr/

COPY --from=builder  /build/usr/local/share/proj/ /usr/local/share/proj/
COPY --from=builder  /build/usr/local/include/ /usr/local/include/
COPY --from=builder  /build/usr/local/bin/ /usr/local/bin/
COPY --from=builder  /build/usr/local/lib/ /usr/local/lib/

COPY --from=builder  /build/usr/share/gdal/ /usr/share/gdal/
COPY --from=builder  /build/usr/include/ /usr/include/
COPY --from=builder  /build/usr/bin/ /usr/bin/
COPY --from=builder  /build/usr/lib/ /usr/lib/

RUN ldconfig
