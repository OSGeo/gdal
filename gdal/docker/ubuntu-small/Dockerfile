##
# osgeo/gdal:ubuntu-small

# This file is available at the option of the licensee under:
# Public domain
# or licensed under X/MIT (LICENSE.TXT) Copyright 2019 Even Rouault <even.rouault@spatialys.com>

FROM ubuntu:18.04 as builder

# Derived from osgeo/proj by Howard Butler <howard@hobu.co>
MAINTAINER Even Rouault <even.rouault@spatialys.com>

ARG DESTDIR="/build"

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
       libcurl4-gnutls-dev libexpat-dev libxerces-c-dev \
       libwebp-dev \
       libzstd1-dev bash zip curl \
       libpq-dev libssl-dev \
       autoconf automake sqlite3 bash-completion

ARG PROJ_VERSION

# Build PROJ
RUN wget -q https://github.com/OSGeo/proj.4/archive/${PROJ_VERSION}.zip -O proj.4-${PROJ_VERSION}.zip \
    && unzip -q proj.4-${PROJ_VERSION}.zip \
    && rm -f proj.4-${PROJ_VERSION}.zip \
    && cd proj.4-${PROJ_VERSION} \
    && ./autogen.sh \
    && CFLAGS='-DPROJ_RENAME_SYMBOLS -O2' CXXFLAGS='-DPROJ_RENAME_SYMBOLS -O2' \
        ./configure --prefix=/usr/local --disable-static \
    && make -j$(nproc) \
    && make install \
    && cd .. \
    && rm -rf proj.4-${PROJ_VERSION} \
    && mv /build/usr/local/lib/libproj.so.15.0.0 /build/usr/local/lib/libinternalproj.so.15.0.0 \
    && ln -s libinternalproj.so.15.0.0 /build/usr/local/lib/libinternalproj.so.15 \
    && ln -s libinternalproj.so.15.0.0 /build/usr/local/lib/libinternalproj.so \
    && rm /build/usr/local/lib/libproj.*  \
    && ln -s libinternalproj.so.15.0.0 /build/usr/local/lib/libproj.so.15 \
    && strip -s /build/usr/local/lib/libinternalproj.so.15.0.0 \
    && for i in /build/usr/local/bin/*; do strip -s $i 2>/dev/null || /bin/true; done

ARG GDAL_VERSION

# Build GDAL
RUN wget -q https://github.com/OSGeo/gdal/archive/${GDAL_VERSION}.zip -O gdal-${GDAL_VERSION}.zip \
    && unzip -q gdal-${GDAL_VERSION}.zip \
    && rm -f gdal-${GDAL_VERSION}.zip \
    && cd gdal-${GDAL_VERSION}/gdal \
    && ./configure --prefix=/usr --without-libtool \
    --with-hide-internal-symbols \
    --with-jpeg12 \
    --with-python \
    --with-webp --with-proj=/build/usr/local \
    --with-libtiff=internal --with-rename-internal-libtiff-symbols \
    --with-geotiff=internal --with-rename-internal-libgeotiff-symbols \
    && make -j$(nproc) \
    && make install \
    && cd ../.. \
    && rm -rf gdal-${GDAL_VERSION} \
    && strip -s /build/usr/lib/libgdal.so.2.5.0 \
    && for i in /build/usr/bin/*; do strip -s $i 2>/dev/null || /bin/true; done

# Build final image
FROM ubuntu:18.04 as runner

RUN date

# PROJ
RUN apt-get update; \
    DEBIAN_FRONTEND=noninteractive apt-get install -y  --no-install-recommends \
        libsqlite3-0 \
        curl unzip

# GDAL
RUN DEBIAN_FRONTEND=noninteractive apt-get install -y  --no-install-recommends \
        libopenjp2-7 python-numpy \
        libjpeg-turbo8 libgeos-3.6.2 libgeos-c1v5 \
        libcurl4 libexpat1 \
        libxerces-c3.2 \
        libwebp6 \
        libzstd1 bash libpq5 libssl1.1

COPY --from=builder  /build/usr/local/bin/ /usr/local/bin/
COPY --from=builder  /build/usr/local/lib/ /usr/local/lib/
COPY --from=builder  /build/usr/local/include/ /usr/local/include/
COPY --from=builder  /build/usr/local/share/proj/ /usr/local/share/proj/

COPY --from=builder  /build/usr/bin/ /usr/bin/
COPY --from=builder  /build/usr/lib/ /usr/lib/
COPY --from=builder  /build/usr/include/ /usr/include/
COPY --from=builder  /build/usr/share/gdal/ /usr/share/gdal/

RUN \
    curl -LOs http://download.osgeo.org/proj/proj-datumgrid-latest.zip && unzip -j -u -o proj-datumgrid-latest.zip  -d /usr/local/share/proj && \
    rm -f *.zip

RUN ldconfig
