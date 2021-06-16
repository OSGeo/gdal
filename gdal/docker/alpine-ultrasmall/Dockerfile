##
# osgeo/gdal:alpine-ultrasmall

# This file is available at the option of the licensee under:
# Public domain
# or licensed under X/MIT (LICENSE.TXT) Copyright 2019 Even Rouault <even.rouault@spatialys.com>

ARG ALPINE_VERSION=3.14
FROM alpine:${ALPINE_VERSION} as builder

# Derived from osgeo/proj by Howard Butler <howard@hobu.co>
LABEL maintainer="Even Rouault <even.rouault@spatialys.com>"

# Setup build env for PROJ
RUN apk add --no-cache wget curl unzip make libtool autoconf automake pkgconfig g++ sqlite sqlite-dev

# For PROJ and GDAL
RUN apk add --no-cache \
    linux-headers \
    curl-dev tiff-dev \
    zlib-dev zstd-dev \
    libjpeg-turbo-dev libpng-dev libwebp-dev openjpeg-dev

# Build openjpeg
#ARG OPENJPEG_VERSION=2.3.1
RUN if test "${OPENJPEG_VERSION}" != ""; then ( \
    apk add --no-cache cmake \
    && wget -q https://github.com/uclouvain/openjpeg/archive/v${OPENJPEG_VERSION}.tar.gz \
    && tar xzf v${OPENJPEG_VERSION}.tar.gz \
    && rm -f v${OPENJPEG_VERSION}.tar.gz \
    && cd openjpeg-${OPENJPEG_VERSION} \
    && cmake . -DBUILD_SHARED_LIBS=ON  -DBUILD_STATIC_LIBS=OFF -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
    && make -j$(nproc) \
    && make install \
    && mkdir -p /build_thirdparty/usr/lib \
    && cp -P /usr/lib/libopenjp2*.so* /build_thirdparty/usr/lib \
    && for i in /build_thirdparty/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && cd .. \
    && rm -rf openjpeg-${OPENJPEG_VERSION} \
    && apk del cmake \
    ); fi

RUN apk add --no-cache rsync ccache

ARG PROJ_DATUMGRID_LATEST_LAST_MODIFIED
RUN \
    mkdir -p /build_projgrids/usr/share/proj \
    && curl -LOs http://download.osgeo.org/proj/proj-datumgrid-latest.zip \
    && unzip -q -j -u -o proj-datumgrid-latest.zip  -d /build_projgrids/usr/share/proj \
    && rm -f *.zip

ARG RSYNC_REMOTE

# Build PROJ
ARG PROJ_VERSION=master
RUN mkdir proj \
    && wget -q https://github.com/OSGeo/PROJ/archive/${PROJ_VERSION}.tar.gz -O - \
        | tar xz -C proj --strip-components=1 \
    && cd proj \
    && ./autogen.sh \
    && if test "${RSYNC_REMOTE}" != ""; then \
        echo "Downloading cache..."; \
        rsync -ra ${RSYNC_REMOTE}/proj/ $HOME/; \
        echo "Finished"; \
        export CC="ccache gcc"; \
        export CXX="ccache g++"; \
        export PROJ_DB_CACHE_DIR="$HOME/.ccache"; \
        ccache -M 100M; \
    fi \
    && ./configure --prefix=/usr --disable-static --enable-lto \
    && make -j$(nproc) \
    && make install \
    && make install DESTDIR="/build_proj" \
    && if test "${RSYNC_REMOTE}" != ""; then \
        ccache -s; \
        echo "Uploading cache..."; \
        rsync -ra --delete $HOME/.ccache ${RSYNC_REMOTE}/proj/; \
        echo "Finished"; \
        rm -rf $HOME/.ccache; \
        unset CC; \
        unset CXX; \
    fi \
    && cd .. \
    && rm -rf proj \
    && for i in /build_proj/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && for i in /build_proj/usr/bin/*; do strip -s $i 2>/dev/null || /bin/true; done

ARG GDAL_VERSION=master
ARG GDAL_RELEASE_DATE
ARG GDAL_BUILD_IS_RELEASE
ARG GDAL_REPOSITORY=OSGeo/gdal

# Build GDAL
RUN if test "${GDAL_VERSION}" = "master"; then \
        export GDAL_VERSION=$(curl -Ls https://api.github.com/repos/${GDAL_REPOSITORY}/commits/HEAD -H "Accept: application/vnd.github.VERSION.sha"); \
        export GDAL_RELEASE_DATE=$(date "+%Y%m%d"); \
    fi \
    && if test "x${GDAL_BUILD_IS_RELEASE}" = "x"; then \
        export GDAL_SHA1SUM=${GDAL_VERSION}; \
    fi \
    && if test "${RSYNC_REMOTE}" != ""; then \
        echo "Downloading cache..."; \
        rsync -ra ${RSYNC_REMOTE}/gdal/ $HOME/; \
        echo "Finished"; \
        export CC="ccache gcc"; \
        export CXX="ccache g++"; \
        ccache -M 1G; \
    fi \
    && mkdir gdal \
    && wget -q https://github.com/${GDAL_REPOSITORY}/archive/${GDAL_VERSION}.tar.gz -O - \
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
    && if test "${RSYNC_REMOTE}" != ""; then \
        ccache -s; \
        echo "Uploading cache..."; \
        rsync -ra --delete $HOME/.ccache ${RSYNC_REMOTE}/gdal/; \
        echo "Finished"; \
        rm -rf $HOME/.ccache; \
        unset CC; \
        unset CXX; \
    fi \
    && cd ../.. \
    && rm -rf gdal \
    && mkdir -p /build_gdal_version_changing/usr/include \
    && mv /build/usr/lib                    /build_gdal_version_changing/usr \
    && mv /build/usr/include/gdal_version.h /build_gdal_version_changing/usr/include \
    && mv /build/usr/bin                    /build_gdal_version_changing/usr \
    && for i in /build_gdal_version_changing/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && for i in /build_gdal_version_changing/usr/bin/*; do strip -s $i 2>/dev/null || /bin/true; done \
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

# Build final image
FROM alpine:${ALPINE_VERSION} as runner

RUN date

RUN apk add --no-cache \
        libstdc++ \
        sqlite-libs \
        libcurl tiff \
        zlib zstd-libs\
        libjpeg-turbo libpng openjpeg libwebp \
    # libturbojpeg.so is not used by GDAL. Only libjpeg.so*
    && rm -f /usr/lib/libturbojpeg.so* \
    # Only libwebp.so is used by GDAL
    && rm -f /usr/lib/libwebpmux.so* /usr/lib/libwebpdemux.so* /usr/lib/libwebpdecoder.so*

# Order layers starting with less frequently varying ones
#COPY --from=builder  /build_thirdparty/usr/ /usr/

COPY --from=builder  /build_projgrids/usr/ /usr/

COPY --from=builder  /build_proj/usr/share/proj/ /usr/share/proj/
COPY --from=builder  /build_proj/usr/include/ /usr/include/
COPY --from=builder  /build_proj/usr/bin/ /usr/bin/
COPY --from=builder  /build_proj/usr/lib/ /usr/lib/

COPY --from=builder  /build/usr/share/gdal/ /usr/share/gdal/
COPY --from=builder  /build/usr/include/ /usr/include/
COPY --from=builder  /build_gdal_version_changing/usr/ /usr/
