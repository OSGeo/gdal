#!/bin/sh

set -e

if test "x${PROJ_VERSION}" = "x"; then
    PROJ_VERSION=master
fi

if test "x${DESTDIR}" = "x"; then
    DESTDIR=/build
fi

set -eu

mkdir proj
wget -q "https://github.com/OSGeo/PROJ/archive/${PROJ_VERSION}.tar.gz" \
    -O - | tar xz -C proj --strip-components=1

(
    cd proj

    if [ -n "${RSYNC_REMOTE:-}" ]; then
        echo "Downloading cache..."
        rsync -ra "${RSYNC_REMOTE}/proj/${GCC_ARCH}/" "$HOME/"
        echo "Finished"

        export CC="ccache ${GCC_ARCH}-linux-gnu-gcc"
        export CXX="ccache ${GCC_ARCH}-linux-gnu-g++"
        export PROJ_DB_CACHE_DIR="$HOME/.ccache"

        ccache -M 100M
    fi

    export CFLAGS="-DPROJ_RENAME_SYMBOLS -O2 -g"
    export CXXFLAGS="-DPROJ_RENAME_SYMBOLS -DPROJ_INTERNAL_CPP_NAMESPACE -O2 -g"

    cmake . \
        -DBUILD_SHARED_LIBS=ON \
        -DCMAKE_INSTALL_PREFIX=${PROJ_INSTALL_PREFIX:-/usr/local} \
        -DBUILD_TESTING=OFF

    make "-j$(nproc)"
    make install DESTDIR="${DESTDIR}"

    if [ -n "${RSYNC_REMOTE:-}" ]; then
        ccache -s

        echo "Uploading cache..."
        rsync -ra --delete "$HOME/.ccache" "${RSYNC_REMOTE}/proj/${GCC_ARCH}/"
        echo "Finished"

        rm -rf "$HOME/.ccache"
        unset CC
        unset CXX
    fi
)

rm -rf proj

if test "${DESTDIR}" = "/build_tmp_proj"; then
    exit 0
fi

PROJ_SO=$(readlink -f "${DESTDIR}${PROJ_INSTALL_PREFIX}/lib/libproj.so" | awk 'BEGIN {FS="libproj.so."} {print $2}')
PROJ_SO_FIRST=$(echo "$PROJ_SO" | awk 'BEGIN {FS="."} {print $1}')
PROJ_SO_DEST="${DESTDIR}${PROJ_INSTALL_PREFIX}/lib/libinternalproj.so.${PROJ_SO}"

mv "${DESTDIR}${PROJ_INSTALL_PREFIX}/lib/libproj.so.${PROJ_SO}" "${PROJ_SO_DEST}"

ln -s "libinternalproj.so.${PROJ_SO}" "${DESTDIR}${PROJ_INSTALL_PREFIX}/lib/libinternalproj.so.${PROJ_SO_FIRST}"
ln -s "libinternalproj.so.${PROJ_SO}" "${DESTDIR}${PROJ_INSTALL_PREFIX}/lib/libinternalproj.so"

rm "${DESTDIR}${PROJ_INSTALL_PREFIX}/lib"/libproj.*
ln -s "libinternalproj.so.${PROJ_SO}" "${DESTDIR}${PROJ_INSTALL_PREFIX}/lib/libproj.so.${PROJ_SO_FIRST}"

if [ "${WITH_DEBUG_SYMBOLS}" = "yes" ]; then
    # separate debug symbols
    mkdir -p "${DESTDIR}${PROJ_INSTALL_PREFIX}/lib/.debug/" "${DESTDIR}${PROJ_INSTALL_PREFIX}/bin/.debug/"

    DEBUG_SO="${DESTDIR}${PROJ_INSTALL_PREFIX}/lib/.debug/libinternalproj.so.${PROJ_SO}.debug"
    ${GCC_ARCH}-linux-gnu-objcopy -v --only-keep-debug --compress-debug-sections "${PROJ_SO_DEST}" "${DEBUG_SO}"
    ${GCC_ARCH}-linux-gnu-strip --strip-debug --strip-unneeded "${PROJ_SO_DEST}"
    ${GCC_ARCH}-linux-gnu-objcopy --add-gnu-debuglink="${DEBUG_SO}" "${PROJ_SO_DEST}"

    for P in "${DESTDIR}${PROJ_INSTALL_PREFIX}/bin"/*; do
        if file -h "$P" | grep -qi elf; then
            F=$(basename "$P")
            DEBUG_P="${DESTDIR}${PROJ_INSTALL_PREFIX}/bin/.debug/${F}.debug"
            ${GCC_ARCH}-linux-gnu-objcopy -v --only-keep-debug --strip-unneeded "$P" "${DEBUG_P}"
            ${GCC_ARCH}-linux-gnu-strip --strip-debug --strip-unneeded "$P"
            ${GCC_ARCH}-linux-gnu-objcopy --add-gnu-debuglink="${DEBUG_P}" "$P"
        fi
    done
else
    ${GCC_ARCH}-linux-gnu-strip -s "${PROJ_SO_DEST}"
    for P in "${DESTDIR}${PROJ_INSTALL_PREFIX}/bin"/*; do
        ${GCC_ARCH}-linux-gnu-strip -s "$P" 2>/dev/null || /bin/true;
    done;
fi
