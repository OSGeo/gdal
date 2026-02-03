# syntax=docker/dockerfile:1

##
# osgeo/gdal:alpine-normal

# This file is available at the option of the licensee under:
# Public domain
# or licensed under MIT (LICENSE.TXT) Copyright 2019 Even Rouault <even.rouault@spatialys.com>

ARG ALPINE_VERSION=3.23
FROM alpine:${ALPINE_VERSION} AS builder

# Derived from osgeo/proj by Howard Butler <howard@hobu.co>
LABEL maintainer="Even Rouault <even.rouault@spatialys.com>"

ENV HOME="/root"

# Setup build env for PROJ
RUN apk add --no-cache curl unzip cmake make libtool autoconf automake pkgconfig gcc g++ sqlite sqlite-dev tiff-dev

ARG PROJ_DATUMGRID_LATEST_LAST_MODIFIED
RUN \
    mkdir -p /build_projgrids/usr/share/proj \
    && curl -LOs http://download.osgeo.org/proj/proj-datumgrid-latest.zip \
    && unzip -q -j -u -o proj-datumgrid-latest.zip  -d /build_projgrids/usr/share/proj \
    && rm -f *.zip

# For PROJ and GDAL
ARG POPPLER_DEV=poppler-dev
RUN apk add --no-cache \
    apache-arrow-dev \
    armadillo-dev \
    basisu-dev \
    blosc-dev \
    brunsli-dev \
    ccache \
    cfitsio-dev \
    cmake \
    curl-dev \
    expat-dev \
    freexl-dev \
    geos-dev \
    git \
    giflib-dev \
    gnu-libiconv-dev \
    hdf5-dev \
    json-c-dev \
    kealib-dev \
    lerc-dev \
    libaec-dev \
    libarchive-dev \
    libavif-dev \
    libdeflate-dev \
    libgeotiff-dev \
    libheif-dev \
    libjpeg-turbo-dev \
    libjxl-dev \
    libkml-dev \
    libpng-dev \
    libpq-dev \
    libspatialite-dev \
    libtirpc-dev \
    libwebp-dev \
    libxml2-dev \
    linux-headers \
    lz4-dev \
    make \
    mariadb-connector-c-dev \
    muparser-dev \
    netcdf-dev \
    ninja-build \
    odbc-cpp-wrapper-dev \
    openexr-dev \
    openjpeg-dev \
    openssl-dev \
    pcre2-dev \
    poppler-dev \
    proj-dev \
    proj-util \
    py3-numpy \
    py3-numpy-dev \
    py3-pip \
    py3-setuptools \
    python3-dev \
    qhull-dev \
    rsync \
    sfcgal-dev \
    snappy-dev \
    sqlite-dev \
    swig \
    tiledb-dev \
    tiff-dev \
    unixodbc-dev \
    xerces-c-dev \
    xz-dev \
    zlib-dev \
    zstd-dev \
    && mkdir -p /build_thirdparty/usr/lib

# Ninja is not in the default path on Alpine.
ENV PATH=/usr/lib/ninja-build/bin:$PATH

RUN if test "$(uname -m)" = "x86_64"; then ( \
    apk add --no-cache librasterlite2-dev \
    ); fi

# Build hdf4
ARG HDF4_VERSION=4.3.1
RUN if test "${HDF4_VERSION}" != "" -a "$(uname -m)" = "x86_64"; then ( \
    apk add --no-cache byacc flex libtirpc-dev \
    && mkdir hdf4 \
    && curl -L -fsS https://github.com/HDFGroup/hdf4/archive/refs/tags/hdf${HDF4_VERSION}.tar.gz \
        | tar xz -C hdf4 --strip-components=1 \
    && cd hdf4 \
    && CFLAGS="-std=c99" LDFLAGS=-ltirpc ./configure --prefix=/usr --enable-shared --disable-static \
        --with-szlib=/usr --disable-fortran --disable-netcdf \
    && make -j$(nproc) \
    && make install \
    && cp -P /usr/lib/libdf*.so* /build_thirdparty/usr/lib \
    && cp -P /usr/lib/libmfhdf*.so* /build_thirdparty/usr/lib \
    && for i in /build_thirdparty/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && cd .. \
    && rm -rf hdf4 \
    && apk del byacc flex libtirpc-dev \
    ); fi

# Build libOpenDRIVE
ARG OPENDRIVE_VERSION=0.6.0-gdal
RUN if test "${OPENDRIVE_VERSION}" != ""; then ( \
    curl -LO -fsS https://github.com/DLR-TS/libOpenDRIVE/archive/refs/tags/${OPENDRIVE_VERSION}.tar.gz \
    && tar xzf ${OPENDRIVE_VERSION}.tar.gz \
    && rm -f ${OPENDRIVE_VERSION}.tar.gz \
    && cd libOpenDRIVE-${OPENDRIVE_VERSION} \
    && cmake . -G Ninja -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr/ \
    && ninja \
    && ninja install \
    && mkdir -p /build_thirdparty/usr/lib \
    && cp -P /usr/lib/libOpenDrive*.so* /build_thirdparty/usr/lib \
    && for i in /build_thirdparty/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && cd .. \
    && rm -rf libOpenDRIVE-${OPENDRIVE_VERSION} \
    ); fi

RUN mkdir proj gdal

ARG RSYNC_REMOTE
ARG WITH_CCACHE

# Build PROJ
ARG PROJ_VERSION=master
RUN --mount=type=cache,id=alpine-normal-proj,target=$HOME/.cache \
    curl -L -fsS https://github.com/OSGeo/PROJ/archive/${PROJ_VERSION}.tar.gz \
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

# Build spatialite
ARG SPATIALITE_VERSION=5.1.0
RUN --mount=type=cache,id=alpine-normal-spatialite,target=$HOME/.cache \
    if test "${SPATIALITE_VERSION}" != "" -a "$(uname -m)" = "x86_64"; then ( \
    curl -LO -fsS https://www.gaia-gis.it/gaia-sins/libspatialite-sources/libspatialite-${SPATIALITE_VERSION}.tar.gz \
    && tar xzf libspatialite-${SPATIALITE_VERSION}.tar.gz \
    && rm -f libspatialite-${SPATIALITE_VERSION}.tar.gz \
    && cd libspatialite-${SPATIALITE_VERSION} \
    && apk add --no-cache minizip-dev \
    && if test "${RSYNC_REMOTE}" != ""; then \
        echo "Downloading cache..."; \
        rsync -ra ${RSYNC_REMOTE}/spatialite/ $HOME/.cache/; \
        echo "Finished"; \
    fi \
    && if [ -n "${WITH_CCACHE:-}" ]; then \
        export CC="ccache gcc"; \
        export CXX="ccache g++"; \
        ccache -M 100M; \
    fi \
    && ./configure --prefix=/usr --disable-static --disable-rttopo \
    && make -j$(nproc) \
    && make install \
    && if test "${RSYNC_REMOTE}" != ""; then \
        echo "Uploading cache..."; \
        rsync -ra --delete $HOME/.cache/ ${RSYNC_REMOTE}/spatialite/; \
        echo "Finished"; \
    fi \
    && if [ -n "${WITH_CCACHE:-}" ]; then \
        ccache -s; \
        unset CC; \
        unset CXX; \
    fi \
    && mkdir -p /build_spatialite/usr/lib \
    && cp -P /usr/lib/libspatialite*.so* /build_spatialite/usr/lib \
    && for i in /build_spatialite/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && cd .. \
    && rm -rf libspatialite-${SPATIALITE_VERSION} \
    ); else \
        mkdir -p /build_spatialite/usr/lib; \
    fi

# Download exprtk
RUN curl -LO -fsS https://github.com/ArashPartow/exprtk/raw/refs/heads/release/exprtk.hpp \
    && mv exprtk.hpp /usr/include

# Build GDAL
ARG GDAL_VERSION=master
ARG GDAL_RELEASE_DATE
ARG GDAL_BUILD_IS_RELEASE
ARG GDAL_REPOSITORY=OSGeo/gdal

COPY --link . gdal/

RUN --mount=type=cache,id=alpine-normal-gdal,target=$HOME/.cache \
    cd gdal \
    && if test "${GDAL_VERSION}" = "master"; then \
        export GDAL_VERSION=$(curl -Ls https://api.github.com/repos/${GDAL_REPOSITORY}/commits/HEAD -H "Accept: application/vnd.github.VERSION.sha"); \
        export GDAL_RELEASE_DATE=$(date "+%Y%m%d"); \
    fi \
    && if test "${GDAL_VERSION}" = "local"; then \
        export GDAL_VERSION=$(git describe --match=BOGUSTAG --always --abbrev=40 --dirty); \
    else \
        rm -rf ./*; \
        (curl -Lo - -fsS https://github.com/${GDAL_REPOSITORY}/archive/${GDAL_VERSION}.tar.gz \
          | tar xz --strip-components=1) || exit 1; \
    fi \
    && if test "x${GDAL_BUILD_IS_RELEASE}" = "x"; then \
        export GDAL_SHA1SUM=${GDAL_VERSION}; \
    fi \
    && if test "${HDF4_VERSION}" != "" -a "$(uname -m)" = "x86_64"; then \
        apk add --no-cache libtirpc-dev \
        && export LDFLAGS="-ltirpc ${LDFLAGS}"; \
    fi \
    && if test "${RSYNC_REMOTE}" != ""; then \
        echo "Downloading cache..."; \
        rsync -ra ${RSYNC_REMOTE}/gdal/$(uname -m)/ $HOME/.cache/; \
        echo "Finished"; \
    fi \
    && if [ -n "${WITH_CCACHE:-}" ]; then \
        # Little trick to avoid issues with Python bindings
        printf "#!/bin/sh\nccache gcc \$*" > ccache_gcc.sh; \
        chmod +x ccache_gcc.sh; \
        printf "#!/bin/sh\nccache g++ \$*" > ccache_g++.sh; \
        chmod +x ccache_g++.sh; \
        export CC=$PWD/ccache_gcc.sh; \
        export CXX=$PWD/ccache_g++.sh; \
        ccache -M 1G; \
    fi \
    && mkdir build \
    && cd build \
    && cmake .. \
        -G Ninja \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_BUILD_TYPE=Release \
        -DGDAL_USE_TIFF_INTERNAL=ON \
        -DGDAL_USE_GEOTIFF_INTERNAL=ON \
        -DIconv_INCLUDE_DIR=/usr/include/gnu-libiconv \
        -DIconv_LIBRARY=/usr/lib/libiconv.so \
        -DBUILD_TESTING=OFF \
        -DOpenDrive_DIR=/usr/lib/ \
        -DOGR_ENABLE_DRIVER_XODR_PLUGIN=TRUE \
        -DGDAL_USE_EXPRTK:BOOL=ON \
    && ninja \
    && DESTDIR="/build" ninja install \
    && (ninja multireadtest && cp apps/multireadtest /build/usr/bin) \
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
    && mkdir -p /build_gdal_python/usr/lib \
    && mkdir -p /build_gdal_python/usr/bin \
    && mkdir -p /build_gdal_version_changing/usr/include \
    && export py_version=$(python3 -c "import sys; print(str(sys.version_info.major) + '.' + str(sys.version_info.minor))") \
    && mv /build/usr/lib/python${py_version} /build_gdal_python/usr/lib \
    && mv /build/usr/lib                    /build_gdal_version_changing/usr \
    && mv /build/usr/include/gdal_version.h /build_gdal_version_changing/usr/include \
    && mv /build/usr/bin/*.py               /build_gdal_python/usr/bin \
    && mv /build/usr/bin                    /build_gdal_version_changing/usr \
    && for i in /build_gdal_version_changing/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && for i in /build_gdal_python/usr/lib/python${py_version}/site-packages/osgeo/*.so; do strip -s $i 2>/dev/null || /bin/true; done \
    && for i in /build_gdal_version_changing/usr/bin/*; do strip -s $i 2>/dev/null || /bin/true; done \
    # Remove resource files of uncompiled drivers
    && (for i in \
            # unused
            /build/usr/share/gdal/*.svg \
            # unused
            /build/usr/share/gdal/*.png \
       ;do rm $i; done)

# Build final image
FROM alpine:${ALPINE_VERSION} AS runner

RUN date

RUN apk add --no-cache \
        armadillo \
        bash-completion \
        basisu \
        blosc \
        brotli-libs \
        brunsli-libs \
        cfitsio \
        expat \
        freexl \
        geos \
        giflib \
        gnu-libiconv-libs \
        hdf5 \
        icu-libs \
        imath \
        mimalloc2 \
        json-c \
        kealib \
        lerc \
        libaec \
        libarchive \
        libarrow \
        libarrow_dataset \
        libavif \
        libcrypto3 \
        libcurl \
        libdeflate \
        libexpat \
        libgcc \
        libheif \
        libjpeg-turbo \
        libjxl \
        libkml \
        libparquet \
        libpng \
        libpq \
        libspatialite \
        libstdc++ \
        liburiparser \
        libwebp \
        libxml2 \
        lz4-libs \
        mariadb-connector-c \
        muparser \
        netcdf \
        minizip \
        musl \
        odbc-cpp-wrapper \
        openexr-libopenexr \
        openjpeg \
        pcre2 \
        poppler \
        libtirpc \
        proj \
        python3 \
        py3-numpy \
        qhull \
        sfcgal \
        snappy \
        sqlite-libs \
        tiff \
        tiledb \
        unixodbc \
        xerces-c \
        xz-libs \
        zlib \
        zstd-libs \
    # libturbojpeg.so is not used by GDAL. Only libjpeg.so*
    && rm -f /usr/lib/libturbojpeg.so* \
    # libpoppler-cpp.so is not used by GDAL. Only libpoppler.so*
    && rm -f /usr/lib/libpoppler-cpp.so* \
    # Only libwebp.so is used by GDAL
    && rm -f /usr/lib/libwebpmux.so* /usr/lib/libwebpdemux.so* /usr/lib/libwebpdecoder.so*

RUN if test "$(uname -m)" = "x86_64"; then ( \
    apk add --no-cache librasterlite2 \
    ); fi

# Order layers starting with less frequently varying ones
COPY --from=builder --link  /build_thirdparty/usr/ /usr/

COPY --from=builder --link  /build_projgrids/usr/ /usr/

COPY --from=builder --link  /build_spatialite/usr/ /usr/

COPY --from=builder --link  /build_proj/usr/share/proj/ /usr/share/proj/
COPY --from=builder --link  /build_proj/usr/include/ /usr/include/
COPY --from=builder --link  /build_proj/usr/bin/ /usr/bin/
COPY --from=builder --link  /build_proj/usr/lib/ /usr/lib/

COPY --from=builder --link  /build/usr/share/bash-completion/ /usr/share/bash-completion/
COPY --from=builder --link  /build/usr/share/gdal/ /usr/share/gdal/
COPY --from=builder --link  /build/usr/include/ /usr/include/
COPY --from=builder --link  /build_gdal_python/usr/ /usr/
COPY --from=builder --link  /build_gdal_version_changing/usr/ /usr/

# Use mimalloc allocator to improve multi-threaded performance
ENV LD_PRELOAD=libmimalloc.so.2

CMD ["/bin/bash", "-l"]
