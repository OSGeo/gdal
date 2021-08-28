#!/bin/sh
set -eu

if [ "${GDAL_VERSION}" = "master" ]; then
    GDAL_VERSION=$(curl -Ls https://api.github.com/repos/${GDAL_REPOSITORY}/commits/HEAD -H "Accept: application/vnd.github.VERSION.sha")
    export GDAL_VERSION
    GDAL_RELEASE_DATE=$(date "+%Y%m%d")
    export GDAL_RELEASE_DATE
fi

if [ -z "${GDAL_BUILD_IS_RELEASE:-}" ]; then
    export GDAL_SHA1SUM=${GDAL_VERSION}
fi

mkdir gdal
wget -q "https://github.com/${GDAL_REPOSITORY}/archive/${GDAL_VERSION}.tar.gz" \
    -O - | tar xz -C gdal --strip-components=1



(
    cd gdal/gdal

    if test "${RSYNC_REMOTE:-}" != ""; then
        echo "Downloading cache..."
        rsync -ra "${RSYNC_REMOTE}/gdal/${GCC_ARCH}/" "$HOME/"
        echo "Finished"

        # Little trick to avoid issues with Python bindings
        printf "#!/bin/sh\nccache %s-linux-gnu-gcc \$*" "${GCC_ARCH}" > ccache_gcc.sh
        chmod +x ccache_gcc.sh
        printf "#!/bin/sh\nccache %s-linux-gnu-g++ \$*" "${GCC_ARCH}" > ccache_g++.sh
        chmod +x ccache_g++.sh
        export CC=$PWD/ccache_gcc.sh
        export CXX=$PWD/ccache_g++.sh

        ccache -M 1G
    fi

    GDAL_CONFIG_OPTS=""

    if echo "$WITH_FILEGDB" | grep -Eiq "^(y(es)?|1|true)$" ; then
      GDAL_CONFIG_OPTS="$GDAL_CONFIG_OPTS  --with-fgdb=/usr/local/FileGDB_API "
      export LD_LIBRARY_PATH=/usr/local/FileGDB_API/lib
    fi

    if echo "$WITH_PDFIUM" | grep -Eiq "^(y(es)?|1|true)$" ; then
      if test "${GCC_ARCH}" = "x86_64"; then
        GDAL_CONFIG_OPTS="$GDAL_CONFIG_OPTS  --with-pdfium=/usr "
      fi
    fi

    if test "${GCC_ARCH}" = "x86_64"; then
      JAVA_ARCH=amd64;
    elif test "${GCC_ARCH}" = "aarch64"; then
      JAVA_ARCH=arm64;
    else
      echo "Unknown arch. FIXME!"
    fi

    if test "${WITH_HOST}" = ""; then
      GDAL_CONFIG_OPTS="$GDAL_CONFIG_OPTS --with-dods-root=/usr "
    fi

    LDFLAGS="-L/build${PROJ_INSTALL_PREFIX-/usr/local}/lib -linternalproj" \
    ./configure --prefix=/usr --sysconfdir=/etc "${WITH_HOST}" \
    --without-libtool \
    --with-hide-internal-symbols \
    --with-jpeg12 \
    --with-python \
    --with-poppler \
    --with-spatialite \
    --with-mysql \
    --with-liblzma \
    --with-webp \
    --with-proj="/build${PROJ_INSTALL_PREFIX-/usr/local}" \
    --with-poppler \
    --with-hdf5 \
    --with-sosi \
    --with-libtiff=internal --with-rename-internal-libtiff-symbols \
    --with-geotiff=internal --with-rename-internal-libgeotiff-symbols \
    --with-kea=/usr/bin/kea-config \
    --with-mongocxxv3 \
    --with-crypto \
    --with-java=/usr/lib/jvm/java-"$JAVA_VERSION"-openjdk-"$JAVA_ARCH" --with-jvm-lib=/usr/lib/jvm/java-"$JAVA_VERSION"-openjdk-"$JAVA_ARCH"/lib/server --with-jvm-lib-add-rpath \
    --with-mdb $GDAL_CONFIG_OPTS

    make "-j$(nproc)"
    make install DESTDIR="/build"

    cd swig/java
    make "-j$(nproc)"
    make install DESTDIR="/build"
    cd ../../

    if [ -n "${RSYNC_REMOTE:-}" ]; then
        ccache -s

        echo "Uploading cache..."
        rsync -ra --delete "$HOME/.ccache" "${RSYNC_REMOTE}/gdal/${GCC_ARCH}/"
        echo "Finished"

        rm -rf "$HOME/.ccache"
        unset CC
        unset CXX
    fi
)

rm -rf gdal
mkdir -p /build_gdal_python/usr/lib
mkdir -p /build_gdal_python/usr/bin
mkdir -p /build_gdal_version_changing/usr/include
mv /build/usr/lib/python3            /build_gdal_python/usr/lib
mv /build/usr/lib                    /build_gdal_version_changing/usr
mv /build/usr/include/gdal_version.h /build_gdal_version_changing/usr/include
mv /build/usr/bin/*.py               /build_gdal_python/usr/bin
mv /build/usr/bin                    /build_gdal_version_changing/usr

if [ "${WITH_DEBUG_SYMBOLS}" = "yes" ]; then
    # separate debug symbols
    for P in /build_gdal_version_changing/usr/lib/* /build_gdal_python/usr/lib/python3/dist-packages/osgeo/*.so /build_gdal_version_changing/usr/bin/*; do
        if file -h "$P" | grep -qi elf; then
            F=$(basename "$P")
            mkdir -p "$(dirname "$P")/.debug"
            DEBUG_P="$(dirname "$P")/.debug/${F}.debug"
            ${GCC_ARCH}-linux-gnu-objcopy -v --only-keep-debug --compress-debug-sections "$P" "${DEBUG_P}"
            ${GCC_ARCH}-linux-gnu-strip --strip-debug --strip-unneeded "$P"
            ${GCC_ARCH}-linux-gnu-objcopy --add-gnu-debuglink="${DEBUG_P}" "$P"
        fi
    done
else
    for P in /build_gdal_version_changing/usr/lib/*; do ${GCC_ARCH}-linux-gnu-strip -s "$P" 2>/dev/null || /bin/true; done
    for P in /build_gdal_python/usr/lib/python3/dist-packages/osgeo/*.so; do ${GCC_ARCH}-linux-gnu-strip -s "$P" 2>/dev/null || /bin/true; done
    for P in /build_gdal_version_changing/usr/bin/*; do ${GCC_ARCH}-linux-gnu-strip -s "$P" 2>/dev/null || /bin/true; done
fi
