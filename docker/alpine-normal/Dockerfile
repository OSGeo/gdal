##
# osgeo/gdal:alpine-normal

# This file is available at the option of the licensee under:
# Public domain
# or licensed under MIT (LICENSE.TXT) Copyright 2019 Even Rouault <even.rouault@spatialys.com>

ARG ALPINE_VERSION=3.19
FROM alpine:${ALPINE_VERSION} as builder

# Derived from osgeo/proj by Howard Butler <howard@hobu.co>
LABEL maintainer="Even Rouault <even.rouault@spatialys.com>"

# Setup build env for PROJ
RUN apk add --no-cache  wget curl unzip cmake make libtool autoconf automake pkgconfig gcc g++ sqlite sqlite-dev tiff-dev

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
    giflib-dev \
    gnu-libiconv-dev \
    hdf5-dev \
    json-c-dev \
    kealib-dev \
    lerc-dev \
    libaec-dev \
    libarchive-dev \
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
    netcdf-dev \
    odbc-cpp-wrapper-dev \
    ogdi-dev \
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

RUN if test "$(uname -m)" = "x86_64"; then ( \
    apk add --no-cache librasterlite2-dev \
    ); fi

# Build hdf4
ARG HDF4_VERSION=4.2.16
RUN if test "${HDF4_VERSION}" != "" -a "$(uname -m)" = "x86_64"; then ( \
    apk add --no-cache byacc flex portablexdr-dev \
    && mkdir hdf4 \
    && wget -q https://support.hdfgroup.org/ftp/HDF/releases/HDF${HDF4_VERSION}/src/hdf-${HDF4_VERSION}.tar.gz -O - \
        | tar xz -C hdf4 --strip-components=2 \
    && cd hdf4 \
    && LDFLAGS=-lportablexdr ./configure --prefix=/usr --enable-shared --disable-static \
        --with-szlib=/usr --disable-fortran --disable-netcdf \
    && make -j$(nproc) \
    && make install \
    && cp -P /usr/lib/libdf*.so* /build_thirdparty/usr/lib \
    && cp -P /usr/lib/libmfhdf*.so* /build_thirdparty/usr/lib \
    && for i in /build_thirdparty/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && cd .. \
    && rm -rf hdf4 \
    && apk del byacc flex portablexdr-dev \
    ); fi

RUN apk add --no-cache rsync ccache
ARG RSYNC_REMOTE

# Build PROJ
ARG PROJ_VERSION=master
RUN mkdir proj \
    && wget -q https://github.com/OSGeo/PROJ/archive/${PROJ_VERSION}.tar.gz -O - \
        | tar xz -C proj --strip-components=1 \
    && cd proj \
    && if test "${RSYNC_REMOTE}" != ""; then \
        echo "Downloading cache..."; \
        rsync -ra ${RSYNC_REMOTE}/proj/$(uname -m)/ $HOME/; \
        echo "Finished"; \
        export CC="ccache gcc"; \
        export CXX="ccache g++"; \
        mkdir -p "$HOME/.ccache"; \
        export PROJ_DB_CACHE_DIR="$HOME/.ccache"; \
        ccache -M 100M; \
    fi \
    # IPO disabled since it crashes with gcc 13.2.1
    && cmake . \
        -DBUILD_SHARED_LIBS=ON \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DENABLE_IPO=OFF \
        -DBUILD_TESTING=OFF \
    && make -j$(nproc) \
    && make install \
    && make install DESTDIR="/build_proj" \
    && if test "${RSYNC_REMOTE}" != ""; then \
        ccache -s; \
        echo "Uploading cache..."; \
        rsync -ra --delete $HOME/.ccache ${RSYNC_REMOTE}/proj/$(uname -m)/; \
        echo "Finished"; \
        rm -rf $HOME/.ccache; \
        unset CC; \
        unset CXX; \
    fi \
    && cd .. \
    && rm -rf proj \
    && for i in /build_proj/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && for i in /build_proj/usr/bin/*; do strip -s $i 2>/dev/null || /bin/true; done

# Build spatialite
ARG SPATIALITE_VERSION=5.1.0
RUN if test "${SPATIALITE_VERSION}" != "" -a "$(uname -m)" = "x86_64"; then ( \
    wget -q https://www.gaia-gis.it/gaia-sins/libspatialite-sources/libspatialite-${SPATIALITE_VERSION}.tar.gz \
    && tar xzf libspatialite-${SPATIALITE_VERSION}.tar.gz \
    && rm -f libspatialite-${SPATIALITE_VERSION}.tar.gz \
    && cd libspatialite-${SPATIALITE_VERSION} \
    && apk add --no-cache minizip-dev \
    && if test "${RSYNC_REMOTE}" != ""; then \
        echo "Downloading cache..."; \
        rsync -ra ${RSYNC_REMOTE}/spatialite/ $HOME/; \
        echo "Finished"; \
        export CC="ccache gcc"; \
        export CXX="ccache g++"; \
        mkdir -p "$HOME/.ccache"; \
        ccache -M 100M; \
    fi \
    && ./configure --prefix=/usr --disable-static --disable-rttopo \
    && make -j$(nproc) \
    && make install \
    && if test "${RSYNC_REMOTE}" != ""; then \
        ccache -s; \
        echo "Uploading cache..."; \
        rsync -ra --delete $HOME/.ccache ${RSYNC_REMOTE}/spatialite/; \
        echo "Finished"; \
        rm -rf $HOME/.ccache; \
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

# Build GDAL
ARG GDAL_VERSION=master
ARG GDAL_RELEASE_DATE
ARG GDAL_BUILD_IS_RELEASE
ARG GDAL_REPOSITORY=OSGeo/gdal

RUN if test "${GDAL_VERSION}" = "master"; then \
        export GDAL_VERSION=$(curl -Ls https://api.github.com/repos/${GDAL_REPOSITORY}/commits/HEAD -H "Accept: application/vnd.github.VERSION.sha"); \
        export GDAL_RELEASE_DATE=$(date "+%Y%m%d"); \
    fi \
    && if test "x${GDAL_BUILD_IS_RELEASE}" = "x"; then \
        export GDAL_SHA1SUM=${GDAL_VERSION}; \
    fi \
    && if test "${HDF4_VERSION}" != "" -a "$(uname -m)" = "x86_64"; then \
        apk add --no-cache portablexdr-dev \
        && export LDFLAGS="-lportablexdr ${LDFLAGS}"; \
    fi \
    && mkdir gdal \
    && wget -q https://github.com/${GDAL_REPOSITORY}/archive/${GDAL_VERSION}.tar.gz -O - \
        | tar xz -C gdal --strip-components=1 \
    && cd gdal \
    && if test "${RSYNC_REMOTE}" != ""; then \
        echo "Downloading cache..."; \
        rsync -ra ${RSYNC_REMOTE}/gdal/$(uname -m)/ $HOME/; \
        echo "Finished"; \
        # Little trick to avoid issues with Python bindings
        printf "#!/bin/sh\nccache gcc \$*" > ccache_gcc.sh; \
        chmod +x ccache_gcc.sh; \
        printf "#!/bin/sh\nccache g++ \$*" > ccache_g++.sh; \
        chmod +x ccache_g++.sh; \
        export CC=$PWD/ccache_gcc.sh; \
        export CXX=$PWD/ccache_g++.sh; \
        mkdir -p "$HOME/.ccache"; \
        ccache -M 1G; \
    fi \
    && mkdir build \
    && cd build \
    && cmake .. \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DCMAKE_BUILD_TYPE=Release \
        -DGDAL_USE_TIFF_INTERNAL=ON \
        -DGDAL_USE_GEOTIFF_INTERNAL=ON \
        -DIconv_INCLUDE_DIR=/usr/include/gnu-libiconv \
        -DIconv_LIBRARY=/usr/lib/libiconv.so \
        -DBUILD_TESTING=OFF \
    && make -j$(nproc) \
    && make install DESTDIR="/build" \
    && (make -j$(nproc) multireadtest && cp apps/multireadtest /build/usr/bin) \
    && cd .. \
    && if test "${RSYNC_REMOTE}" != ""; then \
        ccache -s; \
        echo "Uploading cache..."; \
        rsync -ra --delete $HOME/.ccache ${RSYNC_REMOTE}/gdal/$(uname -m)/; \
        echo "Finished"; \
        rm -rf $HOME/.ccache; \
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
FROM alpine:${ALPINE_VERSION} as runner

RUN date

RUN apk add --no-cache \
        armadillo \
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
        json-c \
        kealib \
        lerc \
        libaec \
        libarchive \
        libarrow \
        libarrow_dataset \
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
        netcdf \
        minizip \
        musl \
        odbc-cpp-wrapper \
        ogdi \
        openexr-libopenexr \
        openjpeg \
        pcre2 \
        poppler \
        portablexdr \
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

# Install Pillow for -antialias option of gdal2tiles
RUN apk add --no-cache py3-pillow

# Order layers starting with less frequently varying ones
COPY --from=builder  /build_thirdparty/usr/ /usr/

COPY --from=builder  /build_projgrids/usr/ /usr/

COPY --from=builder  /build_spatialite/usr/ /usr/

COPY --from=builder  /build_proj/usr/share/proj/ /usr/share/proj/
COPY --from=builder  /build_proj/usr/include/ /usr/include/
COPY --from=builder  /build_proj/usr/bin/ /usr/bin/
COPY --from=builder  /build_proj/usr/lib/ /usr/lib/

COPY --from=builder  /build/usr/share/gdal/ /usr/share/gdal/
COPY --from=builder  /build/usr/include/ /usr/include/
COPY --from=builder  /build_gdal_python/usr/ /usr/
COPY --from=builder  /build_gdal_version_changing/usr/ /usr/
