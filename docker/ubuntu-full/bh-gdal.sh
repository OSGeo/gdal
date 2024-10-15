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
    # -Wno-psabi avoid 'note: parameter passing for argument of type 'std::pair<double, double>' when C++17 is enabled changed to match C++14 in GCC 10.1' on arm64
    export CXXFLAGS="-DPROJ_RENAME_SYMBOLS -DPROJ_INTERNAL_CPP_NAMESPACE -O2 -g -Wno-psabi"

    mkdir build
    cd build
    # GDAL_USE_TIFF_INTERNAL=ON to use JXL
    export GDAL_CMAKE_EXTRA_OPTS=""
    if test "${GCC_ARCH}" != "x86_64"; then
        export GDAL_CMAKE_EXTRA_OPTS="${GDAL_CMAKE_EXTRA_OPTS} -DPDFIUM_INCLUDE_DIR="
    fi
    export JAVA_ARCH=""
    if test "${GCC_ARCH}" = "x86_64"; then
      export JAVA_ARCH="amd64";
    elif test "${GCC_ARCH}" = "aarch64"; then
      export JAVA_ARCH="arm64";
    fi
    if test "${JAVA_ARCH:-}" != ""; then
        export GDAL_CMAKE_EXTRA_OPTS="${GDAL_CMAKE_EXTRA_OPTS} -DBUILD_JAVA_BINDINGS=ON -DJAVA_HOME=/usr/lib/jvm/java-${JAVA_VERSION}-openjdk-${JAVA_ARCH}"
    fi
    if echo "$WITH_FILEGDB" | grep -Eiq "^(y(es)?|1|true)$" ; then
      ln -s /usr/local/FileGDB_API/lib/libFileGDBAPI.so /usr/lib/x86_64-linux-gnu
      export GDAL_CMAKE_EXTRA_OPTS="${GDAL_CMAKE_EXTRA_OPTS} -DFileGDB_ROOT:PATH=/usr/local/FileGDB_API -DFileGDB_LIBRARY:FILEPATH=/usr/lib/x86_64-linux-gnu/libFileGDBAPI.so"
      export LD_LIBRARY_PATH=/usr/local/FileGDB_API/lib:${LD_LIBRARY_PATH:-}
    fi
    echo "${GDAL_CMAKE_EXTRA_OPTS}"
    cmake .. \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DGDAL_FIND_PACKAGE_PROJ_MODE=MODULE \
        -DBUILD_TESTING=OFF \
        -DPROJ_INCLUDE_DIR="/build${PROJ_INSTALL_PREFIX-/usr/local}/include" \
        -DPROJ_LIBRARY="/build${PROJ_INSTALL_PREFIX-/usr/local}/lib/libinternalproj.so" \
        -DGDAL_ENABLE_PLUGINS=ON \
        -DGDAL_USE_TIFF_INTERNAL=ON \
        -DBUILD_PYTHON_BINDINGS=ON \
        -DGDAL_USE_GEOTIFF_INTERNAL=ON ${GDAL_CMAKE_EXTRA_OPTS}

    make "-j$(nproc)"
    make install DESTDIR="/build"

    cd ..

    if [ -n "${RSYNC_REMOTE:-}" ]; then
        ccache -s

        echo "Uploading cache..."
        rsync -ra --delete "$HOME/.cache" "${RSYNC_REMOTE}/gdal/${GCC_ARCH}/"
        echo "Finished"

        rm -rf "$HOME/.cache"
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
    for P in "/build_gdal_version_changing/usr/lib/${GCC_ARCH}-linux-gnu"/* "/build_gdal_version_changing/usr/lib/${GCC_ARCH}-linux-gnu"/gdalplugins/* /build_gdal_python/usr/lib/python3/dist-packages/osgeo/*.so /build_gdal_version_changing/usr/bin/*; do
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
    for P in "/build_gdal_version_changing/usr/lib/${GCC_ARCH}-linux-gnu"/*; do ${GCC_ARCH}-linux-gnu-strip -s "$P" 2>/dev/null || /bin/true; done
    for P in "/build_gdal_version_changing/usr/lib/${GCC_ARCH}-linux-gnu"/gdalplugins/*; do ${GCC_ARCH}-linux-gnu-strip -s "$P" 2>/dev/null || /bin/true; done
    for P in /build_gdal_python/usr/lib/python3/dist-packages/osgeo/*.so; do ${GCC_ARCH}-linux-gnu-strip -s "$P" 2>/dev/null || /bin/true; done
    for P in /build_gdal_version_changing/usr/bin/*; do ${GCC_ARCH}-linux-gnu-strip -s "$P" 2>/dev/null || /bin/true; done
fi
