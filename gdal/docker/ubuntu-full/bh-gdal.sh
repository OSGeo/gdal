#!/bin/sh
set -eu

if [ "${GDAL_VERSION}" = "master" ]; then
    GDAL_VERSION=$(curl -Ls https://api.github.com/repos/OSGeo/gdal/commits/HEAD -H "Accept: application/vnd.github.VERSION.sha")
    export GDAL_VERSION
    GDAL_RELEASE_DATE=$(date "+%Y%m%d")
    export GDAL_RELEASE_DATE
fi

if [ -z "${GDAL_BUILD_IS_RELEASE:-}" ]; then
    export GDAL_SHA1SUM=${GDAL_VERSION}
fi

mkdir gdal
wget -q "https://github.com/OSGeo/gdal/archive/${GDAL_VERSION}.tar.gz" \
    -O - | tar xz -C gdal --strip-components=1



(
    cd gdal/gdal
    if test "${RSYNC_REMOTE:-}" != ""; then
        echo "Downloading cache..."
        rsync -ra "${RSYNC_REMOTE}/gdal/" "$HOME/"
        echo "Finished"

        # Little trick to avoid issues with Python bindings
        printf "#!/bin/sh\nccache gcc \$*" > ccache_gcc.sh
        chmod +x ccache_gcc.sh
        printf "#!/bin/sh\nccache g++ \$*" > ccache_g++.sh
        chmod +x ccache_g++.sh
        export CC=$PWD/ccache_gcc.sh
        export CXX=$PWD/ccache_g++.sh

        ccache -M 1G
    fi

    GDAL_CONFIG_OPTS=""

    if echo "$WITH_FILEGDB" | grep -Eiq "^(y(es)?|1|true)$" ; then
      GDAL_CONFIG_OPTS="$GDAL_CONFIG_OPTS  --with-fgdb=/usr/local/FileGDB_API "
    fi

    if echo "$WITH_PDFIUM" | grep -Eiq "^(y(es)?|1|true)$" ; then
      GDAL_CONFIG_OPTS="$GDAL_CONFIG_OPTS  --with-pdfium=/usr "
    fi

    LDFLAGS="-L/build${PROJ_INSTALL_PREFIX-/usr/local}/lib -linternalproj" ./configure --prefix=/usr \
    --without-libtool \
    --with-hide-internal-symbols \
    --with-jpeg12 \
    --with-python \
    --with-poppler \
    --with-spatialite \
    --with-mysql \
    --with-liblzma \
    --with-webp \
    --with-epsilon \
    --with-proj="/build${PROJ_INSTALL_PREFIX-/usr/local}" \
    --with-poppler \
    --with-hdf5 \
    --with-dods-root=/usr \
    --with-sosi \
    --with-libtiff=internal --with-rename-internal-libtiff-symbols \
    --with-geotiff=internal --with-rename-internal-libgeotiff-symbols \
    --with-kea=/usr/bin/kea-config \
    --with-mongocxxv3 \
    --with-tiledb \
    --with-crypto \
    --with-java=/usr/lib/jvm/java-"$JAVA_VERSION"-openjdk-amd64 --with-jvm-lib=/usr/lib/jvm/java-"$JAVA_VERSION"-openjdk-amd64/lib/server --with-jvm-lib-add-rpath \
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
        rsync -ra --delete "$HOME/.ccache" "${RSYNC_REMOTE}/gdal/"
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
            objcopy -v --only-keep-debug --compress-debug-sections "$P" "${DEBUG_P}"
            strip --strip-debug --strip-unneeded "$P"
            objcopy --add-gnu-debuglink="${DEBUG_P}" "$P"
        fi
    done
else
    for P in /build_gdal_version_changing/usr/lib/*; do strip -s "$P" 2>/dev/null || /bin/true; done
    for P in /build_gdal_python/usr/lib/python3/dist-packages/osgeo/*.so; do strip -s "$P" 2>/dev/null || /bin/true; done
    for P in /build_gdal_version_changing/usr/bin/*; do strip -s "$P" 2>/dev/null || /bin/true; done
fi
