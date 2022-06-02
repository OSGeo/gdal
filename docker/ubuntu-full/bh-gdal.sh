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
    cd gdal

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

    export CFLAGS="-DPROJ_RENAME_SYMBOLS -O2 -g"
    export CXXFLAGS="-DPROJ_RENAME_SYMBOLS -DPROJ_INTERNAL_CPP_NAMESPACE -O2 -g"

    mkdir build
    cd build
    # GDAL_USE_TIFF_INTERNAL=ON to use JXL
    export GDAL_CMAKE_EXTRA_OPTS=""
    if test "${GCC_ARCH}" != "x86_64"; then
        export GDAL_CMAKE_EXTRA_OPTS="${GDAL_CMAKE_EXTRA_OPTS} -DPDFIUM_INCLUDE_DIR="
    fi
    if test "${TARGET_ARCH:-}" != ""; then
        export JDK_PATH="/usr/lib/jvm/java-${JAVA_VERSION}-openjdk-${TARGET_ARCH}"
        export GDAL_CMAKE_EXTRA_OPTS="${GDAL_CMAKE_EXTRA_OPTS} -DJAVA_AWT_INCLUDE_PATH:PATH=${JDK_PATH}/include -DJAVA_AWT_LIBRARY:FILEPATH=${JDK_PATH}/lib/libjawt.so -DJAVA_INCLUDE_PATH:PATH=${JDK_PATH}/include -DJAVA_INCLUDE_PATH2:PATH=${JDK_PATH}/include/linux -DJAVA_JVM_LIBRARY:FILEPATH=${JDK_PATH}/lib/server/libjvm.so"
    fi
    cmake .. \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DPROJ_INCLUDE_DIR="/build${PROJ_INSTALL_PREFIX-/usr/local}/include" \
        -DPROJ_LIBRARY="/build${PROJ_INSTALL_PREFIX-/usr/local}/lib/libinternalproj.so" \
        -DGDAL_USE_TIFF_INTERNAL=ON \
        -DGDAL_USE_GEOTIFF_INTERNAL=ON ${GDAL_CMAKE_EXTRA_OPTS}

    make "-j$(nproc)"
    make install DESTDIR="/build"

    cd ..

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
