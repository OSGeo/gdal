##
# osgeo/gdal:alpine-ultrasmall

# This file is available at the option of the licensee under:
# Public domain
# or licensed under X/MIT (LICENSE.TXT) Copyright 2019 Even Rouault <even.rouault@spatialys.com>

FROM alpine:latest as builder

# Derived from osgeo/proj by Howard Butler <howard@hobu.co>
MAINTAINER Even Rouault <even.rouault@spatialys.com>

# Setup build env for PROJ
RUN apk add --no-cache wget curl unzip make libtool autoconf automake pkgconfig g++ sqlite sqlite-dev

# For GDAL
RUN apk add --no-cache \
    linux-headers \
    curl-dev \
    zlib-dev zstd-dev \
    libjpeg-turbo-dev libpng-dev openjpeg-dev libwebp-dev

ARG PROJ_VERSION=master

# Build PROJ
RUN mkdir proj \
    && wget -q https://github.com/OSGeo/proj.4/archive/${PROJ_VERSION}.tar.gz -O - \
        | tar xz -C proj --strip-components=1 \
    && cd proj \
    && ./autogen.sh \
    && ./configure --prefix=/usr --disable-static --enable-lto \
    && make -j$(nproc) \
    && make install \
    && make install DESTDIR="/build_proj" \
    && cd .. \
    && rm -rf proj \
    && for i in /build_proj/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && for i in /build_proj/usr/bin/*; do strip -s $i 2>/dev/null || /bin/true; done

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
    --with-proj=/usr \
    --with-libtiff=internal --with-rename-internal-libtiff-symbols \
    --with-geotiff=internal --with-rename-internal-libgeotiff-symbols \
    --disable-all-optional-drivers \
    --enable-driver-shape \
    --enable-driver-gpkg \
    --with-webp \
    --without-jpeg12 \
    --without-pcraster \
    --without-pcidsk \
    --without-lerc \
    --without-gnm \
    --without-gif \
    --enable-lto \
    && make -j$(nproc) \
    && make install DESTDIR="/build" \
    && cd ../.. \
    && rm -rf gdal \
    && for i in /build/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && for i in /build/usr/bin/*; do strip -s $i 2>/dev/null || /bin/true; done \
    # Remove resource files of uncompiled drivers
    && (for i in \
            # BAG driver
            /build/usr/share/gdal/bag*.xml \
            # SXF driver
            /build/usr/share/gdal/default.rsc \
            # unused
            /build/usr/share/gdal/*.svg \
            # unused
            /build/usr/share/gdal/*.png \
            # GML driver
            /build/usr/share/gdal/*.gfs \
            # GML driver
            /build/usr/share/gdal/gml_registry.xml \
            # NITF driver
            /build/usr/share/gdal/nitf* \
            # NITF driver
            /build/usr/share/gdal/gt_datum.csv \
            # NITF driver
            /build/usr/share/gdal/gt_ellips.csv \
            # PDF driver
            /build/usr/share/gdal/pdf* \
            # PDS4 driver
            /build/usr/share/gdal/pds* \
            # S57 driver
            /build/usr/share/gdal/s57* \
            # VDV driver
            /build/usr/share/gdal/vdv* \
            # DXF driver
            /build/usr/share/gdal/*.dxf \
            # DGN driver
            /build/usr/share/gdal/*.dgn \
            # OSM driver
            /build/usr/share/gdal/osm* \
            # GMLAS driver
            /build/usr/share/gdal/gmlas* \
            # PLScenes driver
            /build/usr/share/gdal/plscenes* \
            # netCDF driver
            /build/usr/share/gdal/netcdf_config.xsd \
            # PCIDSK driver
            /build/usr/share/gdal/pci* \
            # ECW and ERS drivers
            /build/usr/share/gdal/ecw_cs.wkt \
            # EEDA driver
            /build/usr/share/gdal/eedaconf.json \
            # MAP driver / ImportFromOZI()
            /build/usr/share/gdal/ozi_* \
       ;do rm $i; done)

RUN \
    mkdir -p /build_projgrids/usr/share/proj \
    && curl -LOs http://download.osgeo.org/proj/proj-datumgrid-latest.zip \
    && unzip -q -j -u -o proj-datumgrid-latest.zip  -d /build_projgrids/usr/share/proj \
    && rm -f *.zip

# Build final image
FROM alpine:latest as runner

RUN date

RUN apk add --no-cache \
        libstdc++ \
        sqlite-libs \
        libcurl \
        zlib zstd-libs\
        libjpeg-turbo libpng openjpeg libwebp \
    # libturbojpeg.so is not used by GDAL. Only libjpeg.so*
    && rm -f /usr/lib/libturbojpeg.so* \
    # Only libwebp.so is used by GDAL
    && rm -f /usr/lib/libwebpmux.so* /usr/lib/libwebpdemux.so* /usr/lib/libwebpdecoder.so*

# Order layers starting with less frequently varying ones
COPY --from=builder  /build_projgrids/usr/ /usr/

COPY --from=builder  /build_proj/usr/share/proj/ /usr/share/proj/
COPY --from=builder  /build_proj/usr/include/ /usr/include/
COPY --from=builder  /build_proj/usr/bin/ /usr/bin/
COPY --from=builder  /build_proj/usr/lib/ /usr/lib/

COPY --from=builder  /build/usr/share/gdal/ /usr/share/gdal/
COPY --from=builder  /build/usr/include/ /usr/include/
COPY --from=builder  /build/usr/bin/ /usr/bin/
COPY --from=builder  /build/usr/lib/ /usr/lib/
