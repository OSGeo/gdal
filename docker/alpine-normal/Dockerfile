##
# osgeo/gdal:alpine-normal

# This file is available at the option of the licensee under:
# Public domain
# or licensed under MIT (LICENSE.TXT) Copyright 2019 Even Rouault <even.rouault@spatialys.com>

ARG ALPINE_VERSION=3.15
FROM alpine:${ALPINE_VERSION} as builder

# Derived from osgeo/proj by Howard Butler <howard@hobu.co>
LABEL maintainer="Even Rouault <even.rouault@spatialys.com>"

# Setup build env for PROJ
RUN apk add --no-cache  wget curl unzip make libtool autoconf automake pkgconfig g++ sqlite sqlite-dev

ARG PROJ_DATUMGRID_LATEST_LAST_MODIFIED
RUN \
    mkdir -p /build_projgrids/usr/share/proj \
    && curl -LOs http://download.osgeo.org/proj/proj-datumgrid-latest.zip \
    && unzip -q -j -u -o proj-datumgrid-latest.zip  -d /build_projgrids/usr/share/proj \
    && rm -f *.zip

# For PROJ and GDAL
ARG POPPLER_DEV=poppler-dev
RUN apk add --no-cache \
    linux-headers \
    curl-dev tiff-dev \
    zlib-dev zstd-dev xz-dev snappy-dev \
    libjpeg-turbo-dev libpng-dev openjpeg-dev libwebp-dev expat-dev \
    py3-numpy-dev python3-dev py3-setuptools py3-numpy \
    ${POPPLER_DEV} postgresql-dev \
    openexr-dev libheif-dev xerces-c-dev geos-dev cfitsio-dev \
    netcdf-dev hdf5-dev freexl-dev \
    lz4-dev \
    # For spatialite (and GDAL)
    libxml2-dev \
    && mkdir -p /build_thirdparty/usr/lib

# Build blosc
ARG BLOSC_VERSION=1.21.0
RUN if test "${BLOSC_VERSION}" != ""; then ( \
    apk add --no-cache cmake \
    && wget -q https://github.com/Blosc/c-blosc/archive/refs/tags/v${BLOSC_VERSION}.tar.gz \
    && tar xzf v${BLOSC_VERSION}.tar.gz \
    && rm -f v${BLOSC_VERSION}.tar.gz \
    && cd c-blosc-${BLOSC_VERSION} \
    && cmake . \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DBUILD_SHARED=ON \
        -DBUILD_STATIC=OFF \
        -DBUILD_BENCHMARKS=OFF \
        -DBUILD_FUZZERS=OFF \
        -DBUILD_TESTS=OFF \
        -DPREFER_EXTERNAL_LZ4=ON \
        -DPREFER_EXTERNAL_ZLIB=ON \
        -DPREFER_EXTERNAL_ZSTD=ON \
        -DDEACTIVATE_SNAPPY=OFF \
    && make -j$(nproc) \
    && make install \
    && cp -P /usr/lib/libblosc*.so* /build_thirdparty/usr/lib \
    && for i in /build_thirdparty/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && cd .. \
    && rm -rf c-blosc-${BLOSC_VERSION} \
    ); fi

# Build szip
ARG SZIP_VERSION=2.1.1
RUN if test "${SZIP_VERSION}" != ""; then ( \
    wget -q https://support.hdfgroup.org/ftp/lib-external/szip/${SZIP_VERSION}/src/szip-${SZIP_VERSION}.tar.gz \
    && tar xzf szip-${SZIP_VERSION}.tar.gz \
    && rm -f szip-${SZIP_VERSION}.tar.gz \
    && cd szip-${SZIP_VERSION} \
    && CFLAGS=-O2 ./configure --prefix=/usr --disable-static \
    && make -j$(nproc) \
    && make install \
    && cp -P /usr/lib/libsz*.so* /build_thirdparty/usr/lib \
    && for i in /build_thirdparty/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && cd .. \
    && rm -rf szip-${SZIP_VERSION} \
    ); fi

# Build hdf4
ARG HDF4_VERSION=4.2.15
RUN if test "${HDF4_VERSION}" != "" -a "$(uname -m)" = "x86_64"; then ( \
    apk add --no-cache byacc flex portablexdr-dev \
    && mkdir hdf4 \
    && wget -q https://support.hdfgroup.org/ftp/HDF/releases/HDF${HDF4_VERSION}/src/hdf-${HDF4_VERSION}.tar.gz -O - \
        | tar xz -C hdf4 --strip-components=1 \
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

# Build likbkea
ARG KEA_VERSION=1.4.13
RUN if test "${KEA_VERSION}" != ""; then ( \
    apk add --no-cache cmake \
    && wget -q https://github.com/ubarsc/kealib/archive/kealib-${KEA_VERSION}.zip \
    && unzip -q kealib-${KEA_VERSION}.zip \
    && rm -f kealib-${KEA_VERSION}.zip \
    && cd kealib-kealib-${KEA_VERSION} \
    && cmake . -DBUILD_SHARED_LIBS=ON -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr -DHDF5_INCLUDE_DIR=/usr/include/hdf5 \
        -DHDF5_LIB_PATH=/usr/lib -DLIBKEA_WITH_GDAL=OFF \
    && make -j$(nproc) \
    && make install \
    && cd .. \
    && rm -rf kealib-kealib-${KEA_VERSION} \
    && cp -P /usr/lib/libkea*.so* /build_thirdparty/usr/lib \
    && for i in /build_thirdparty/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && apk del cmake \
    ); fi

# Build openjpeg
# ARG OPENJPEG_VERSION=2.3.1
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
    && rm -f /usr/lib/libopenjp2.so.2.3.0 \
    && cp -P /usr/lib/libopenjp2*.so* /build_thirdparty/usr/lib \
    && for i in /build_thirdparty/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && cd .. \
    && rm -rf openjpeg-${OPENJPEG_VERSION} \
    && apk del cmake \
    ); fi

# Build libdeflate
ARG LIBDEFLATE_VERSION=1.7
RUN if test "${LIBDEFLATE_VERSION}" != ""; then ( \
    mkdir libdeflate \
    && wget -q https://github.com/ebiggers/libdeflate/archive/v${LIBDEFLATE_VERSION}.tar.gz -O - \
        | tar xz -C libdeflate --strip-components=1 \
    && cd libdeflate \
    && make -j$(nproc) PREFIX=/usr install \
    && cp -P /usr/lib/libdeflate.so* /build_thirdparty/usr/lib \
    && for i in /build_thirdparty/usr/lib/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && cd .. \
    && rm -rf libdeflate \
    ); fi

RUN apk add --no-cache rsync ccache
ARG RSYNC_REMOTE

# Build PROJ
ARG PROJ_VERSION=master
RUN mkdir proj \
    && apk add --no-cache cmake \
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
    && cmake . \
        -DBUILD_SHARED_LIBS=ON \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DENABLE_IPO=ON \
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
    && for i in /build_proj/usr/bin/*; do strip -s $i 2>/dev/null || /bin/true; done \
    && apk del cmake

# Build spatialite
ARG SPATIALITE_VERSION=5.0.1
RUN if test "${SPATIALITE_VERSION}" != "" -a "$(uname -m)" = "x86_64"; then ( \
    wget -q http://www.gaia-gis.it/gaia-sins/libspatialite-${SPATIALITE_VERSION}.tar.gz \
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
    && apk add --no-cache cmake \
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
       ;do rm $i; done)\
    && apk del cmake

# Build final image
FROM alpine:${ALPINE_VERSION} as runner

RUN date

ARG POPPLER=poppler
RUN apk add --no-cache \
        libstdc++ \
        sqlite-libs \
        libcurl tiff \
        zlib zstd-libs xz-libs snappy \
        libjpeg-turbo libpng libwebp expat \
        icu-libs \
        python3 py3-numpy ${POPPLER} pcre libpq libxml2 portablexdr openjpeg \
        netcdf hdf5 freexl \
        openexr libheif xerces-c geos cfitsio minizip \
        lz4-libs \
    # Remove /usr/lib/libopenjp2.so.2.3.0 since we are building v2.3.1 manually
    # && rm -f /usr/lib/libopenjp2.so.2.3.0 \
    # libturbojpeg.so is not used by GDAL. Only libjpeg.so*
    && rm -f /usr/lib/libturbojpeg.so* \
    # libpoppler-cpp.so is not used by GDAL. Only libpoppler.so*
    && rm -f /usr/lib/libpoppler-cpp.so* \
    # Only libwebp.so is used by GDAL
    && rm -f /usr/lib/libwebpmux.so* /usr/lib/libwebpdemux.so* /usr/lib/libwebpdecoder.so* \
    && ln -s /usr/bin/python3 /usr/bin/python

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
