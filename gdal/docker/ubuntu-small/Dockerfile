##
# osgeo/gdal:ubuntu-small

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
       libopenjp2-7-dev \
       python-dev python-numpy \
       libjpeg-dev libgeos-dev \
       curl libcurl4-gnutls-dev libexpat-dev libxerces-c-dev \
       libwebp-dev \
       libzstd1-dev bash zip curl \
       libpq-dev libssl-dev \
       autoconf automake sqlite3 bash-completion

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
    --with-python \
    --with-webp --with-proj=/build/usr/local \
    --with-libtiff=internal --with-rename-internal-libtiff-symbols \
    --with-geotiff=internal --with-rename-internal-libgeotiff-symbols \
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
    DEBIAN_FRONTEND=noninteractive apt-get install -y  --no-install-recommends \
        libsqlite3-0 \
        curl unzip

# GDAL dependencies
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y  --no-install-recommends \
        libopenjp2-7 python-numpy \
        libjpeg-turbo8 libgeos-3.6.2 libgeos-c1v5 \
        libcurl4 libexpat1 \
        libxerces-c3.2 \
        libwebp6 \
        libzstd1 bash libpq5 libssl1.1

# Order layers starting with less frequently varying ones
RUN \
    mkdir -p  /usr/local/share/proj \
    && curl -LOs http://download.osgeo.org/proj/proj-datumgrid-latest.zip \
    && unzip -j -u -o proj-datumgrid-latest.zip  -d /usr/local/share/proj \
    && rm -f *.zip

COPY --from=builder  /build/usr/local/share/proj/ /usr/local/share/proj/
COPY --from=builder  /build/usr/local/include/ /usr/local/include/
COPY --from=builder  /build/usr/local/bin/ /usr/local/bin/
COPY --from=builder  /build/usr/local/lib/ /usr/local/lib/

COPY --from=builder  /build/usr/share/gdal/ /usr/share/gdal/
COPY --from=builder  /build/usr/include/ /usr/include/
COPY --from=builder  /build/usr/bin/ /usr/bin/
COPY --from=builder  /build/usr/lib/ /usr/lib/

RUN ldconfig
