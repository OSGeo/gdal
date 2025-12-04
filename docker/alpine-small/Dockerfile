# syntax=docker/dockerfile:1

##
# osgeo/gdal:alpine-small

# This file is available at the option of the licensee under:
# Public domain
# or licensed under MIT (LICENSE.TXT) Copyright 2019 Even Rouault <even.rouault@spatialys.com>

ARG ALPINE_VERSION=3.23
FROM alpine:${ALPINE_VERSION} AS builder

# Derived from osgeo/proj by Howard Butler <howard@hobu.co>
LABEL maintainer="Even Rouault <even.rouault@spatialys.com>"

ENV HOME="/root"

# Setup build env for PROJ
RUN apk add --no-cache curl unzip make libtool autoconf automake pkgconfig g++ sqlite sqlite-dev

# For PROJ and GDAL
RUN apk add --no-cache \
    ccache \
    cmake ninja-build \
    linux-headers \
    curl-dev tiff-dev \
    zlib-dev zstd-dev lz4-dev libdeflate-dev libarchive-dev \
    libjpeg-turbo-dev libpng-dev libwebp-dev expat-dev \
    postgresql-dev \
    rsync \
    openjpeg-dev \
    muparser-dev

# Ninja is not in the default path on Alpine.
ENV PATH=/usr/lib/ninja-build/bin:$PATH

# Build openjpeg
#ARG OPENJPEG_VERSION=2.3.1
RUN if test "${OPENJPEG_VERSION}" != ""; then ( \
    curl -LO -fsS https://github.com/uclouvain/openjpeg/archive/v${OPENJPEG_VERSION}.tar.gz \
    && tar xzf v${OPENJPEG_VERSION}.tar.gz \
    && rm -f v${OPENJPEG_VERSION}.tar.gz \
    && cd openjpeg-${OPENJPEG_VERSION} \
    && cmake . -G Ninja -DBUILD_SHARED_LIBS=ON  -DBUILD_STATIC_LIBS=OFF -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
    && ninja \
    && ninja install \
    && mkdir -p /build_thirdparty/usr/lib \
    && cp -P /usr/lib/libopenjp2*.so* /build_thirdparty/usr/lib \
    && for i in /build_thirdparty/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && cd .. \
    && rm -rf openjpeg-${OPENJPEG_VERSION} \
    ); fi

ARG PROJ_DATUMGRID_LATEST_LAST_MODIFIED
RUN \
    mkdir -p /build_projgrids/usr/share/proj \
    && curl -LO -fsS http://download.osgeo.org/proj/proj-datumgrid-latest.zip \
    && unzip -q -j -u -o proj-datumgrid-latest.zip  -d /build_projgrids/usr/share/proj \
    && rm -f *.zip

ARG RSYNC_REMOTE
ARG WITH_CCACHE

# Build PROJ
ARG PROJ_VERSION=master
RUN --mount=type=cache,id=alpine-small-proj,target=$HOME/.cache \
    mkdir proj \
    && curl -L -fsS https://github.com/OSGeo/PROJ/archive/${PROJ_VERSION}.tar.gz \
        | tar xz -C proj --strip-components=1 \
    && cd proj \
    && if test "${RSYNC_REMOTE}" != ""; then \
        echo "Downloading cache..."; \
        rsync -ra ${RSYNC_REMOTE}/proj/$(uname -m)/ $HOME/.cache/; \
        echo "Finished"; \
    fi \
    && if [ -n "${WITH_CCACHE:-}" ]; then \
        export CC="ccache gcc"; \
        export CXX="ccache g++"; \
        export PROJ_DB_CACHE_PARAM="-DPROJ_DB_CACHE_DIR=$HOME/.cache"; \
        ccache -M 100M; \
    fi \
    # IPO disabled since it crashes with gcc 13.2.1
    && cmake . \
        -G Ninja \
        -DBUILD_SHARED_LIBS=ON \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DENABLE_IPO=OFF \
        -DBUILD_TESTING=OFF \
        $PROJ_DB_CACHE_PARAM \
    && ninja \
    && ninja install \
    && DESTDIR="/build_proj" ninja install \
    && if test "${RSYNC_REMOTE}" != ""; then \
        echo "Uploading cache..."; \
        rsync -ra --delete $HOME/.cache/ ${RSYNC_REMOTE}/proj/$(uname -m)/; \
        echo "Finished"; \
    fi \
    && if [ -n "${WITH_CCACHE:-}" ]; then \
        ccache -s; \
        unset CC; \
        unset CXX; \
    fi \
    && cd .. \
    && rm -rf proj \
    && for i in /build_proj/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && for i in /build_proj/usr/bin/*; do strip -s $i 2>/dev/null || /bin/true; done

# Build GDAL
ARG GDAL_VERSION=master
ARG GDAL_RELEASE_DATE
ARG GDAL_BUILD_IS_RELEASE
ARG GDAL_REPOSITORY=OSGeo/gdal

RUN --mount=type=cache,id=alpine-small-gdal,target=$HOME/.cache \
    if test "${GDAL_VERSION}" = "master"; then \
        export GDAL_VERSION=$(curl -Ls https://api.github.com/repos/${GDAL_REPOSITORY}/commits/HEAD -H "Accept: application/vnd.github.VERSION.sha"); \
        export GDAL_RELEASE_DATE=$(date "+%Y%m%d"); \
    fi \
    && if test "x${GDAL_BUILD_IS_RELEASE}" = "x"; then \
        export GDAL_SHA1SUM=${GDAL_VERSION}; \
    fi \
    && if test "${RSYNC_REMOTE}" != ""; then \
        echo "Downloading cache..."; \
        rsync -ra ${RSYNC_REMOTE}/gdal/$(uname -m)/ $HOME/.cache/; \
        echo "Finished"; \
    fi \
    && if [ -n "${WITH_CCACHE:-}" ]; then \
        export CC="ccache gcc"; \
        export CXX="ccache g++"; \
        ccache -M 1G; \
    fi \
    && mkdir gdal \
    && curl -L -fsS https://github.com/${GDAL_REPOSITORY}/archive/${GDAL_VERSION}.tar.gz \
        | tar xz -C gdal --strip-components=1 \
    && cd gdal \
    && mkdir build \
    && cd build \
    && cmake .. \
        -G Ninja \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_BUILD_TYPE=Release \
        -DGDAL_USE_TIFF_INTERNAL=ON \
        -DGDAL_USE_GEOTIFF_INTERNAL=ON \
        -DBUILD_TESTING=OFF \
    && ninja \
    && DESTDIR="/build" ninja install \
    && cd .. \
    && if test "${RSYNC_REMOTE}" != ""; then \
        echo "Uploading cache..."; \
        rsync -ra --delete $HOME/.cache/ ${RSYNC_REMOTE}/gdal/$(uname -m)/; \
        echo "Finished"; \
    fi \
    && if [ -n "${WITH_CCACHE:-}" ]; then \
        ccache -s; \
        unset CC; \
        unset CXX; \
    fi \
    && cd .. \
    && rm -rf gdal \
    && mkdir -p /build_gdal_version_changing/usr/include \
    && mv /build/usr/lib                    /build_gdal_version_changing/usr \
    && mv /build/usr/include/gdal_version.h /build_gdal_version_changing/usr/include \
    && mv /build/usr/bin                    /build_gdal_version_changing/usr \
    && for i in /build_gdal_version_changing/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && for i in /build_gdal_version_changing/usr/bin/*; do strip -s $i 2>/dev/null || /bin/true; done

# Build final image
FROM alpine:${ALPINE_VERSION} AS runner

RUN date

RUN apk add --no-cache \
        libstdc++ \
        bash-completion \
        sqlite-libs \
        libcurl tiff \
        zlib zstd-libs lz4-libs libdeflate libarchive \
        libjpeg-turbo libpng openjpeg libwebp expat libpq muparser mimalloc2 \
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

COPY --from=builder  /build/usr/share/bash-completion/ /usr/share/bash-completion/
COPY --from=builder  /build/usr/share/gdal/ /usr/share/gdal/
COPY --from=builder  /build/usr/include/ /usr/include/
COPY --from=builder  /build_gdal_version_changing/usr/ /usr/

# Use mimalloc allocator to improve multi-threaded performance
ENV LD_PRELOAD=libmimalloc.so.2

CMD ["/bin/bash", "-l"]
