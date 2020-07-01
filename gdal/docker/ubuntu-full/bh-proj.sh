#!/bin/sh
set -eu

mkdir proj
wget -q "https://github.com/OSGeo/PROJ/archive/${PROJ_VERSION}.tar.gz" \
    -O - | tar xz -C proj --strip-components=1

(
    cd proj

    ./autogen.sh

    if [ -n "${RSYNC_REMOTE:-}" ]; then
        echo "Downloading cache..."
        rsync -ra "${RSYNC_REMOTE}/proj/" "$HOME/"
        echo "Finished"

        export CC="ccache gcc"
        export CXX="ccache g++"
        export PROJ_DB_CACHE_DIR="$HOME/.ccache"

        ccache -M 100M
    fi

    export CFLAGS="-DPROJ_RENAME_SYMBOLS -O2 -g"
    export CXXFLAGS="-DPROJ_RENAME_SYMBOLS -DPROJ_INTERNAL_CPP_NAMESPACE -O2 -g"

    ./configure "--prefix=${PROJ_INSTALL_PREFIX:-/usr/local}" --disable-static

    make "-j$(nproc)"
    make install DESTDIR="/build"

    if [ -n "${RSYNC_REMOTE:-}" ]; then
        ccache -s

        echo "Uploading cache..."
        rsync -ra --delete "$HOME/.ccache" "${RSYNC_REMOTE}/proj/"
        echo "Finished"

        rm -rf "$HOME/.ccache"
        unset CC
        unset CXX
    fi
)

rm -rf proj

PROJ_SO=$(readlink "/build${PROJ_INSTALL_PREFIX}/lib/libproj.so" | sed "s/libproj\.so\.//")
PROJ_SO_FIRST=$(echo "$PROJ_SO" | awk 'BEGIN {FS="."} {print $1}')
PROJ_SO_DEST="/build${PROJ_INSTALL_PREFIX}/lib/libinternalproj.so.${PROJ_SO}"

mv "/build${PROJ_INSTALL_PREFIX}/lib/libproj.so.${PROJ_SO}" "${PROJ_SO_DEST}"

ln -s "libinternalproj.so.${PROJ_SO}" "/build${PROJ_INSTALL_PREFIX}/lib/libinternalproj.so.${PROJ_SO_FIRST}"
ln -s "libinternalproj.so.${PROJ_SO}" "/build${PROJ_INSTALL_PREFIX}/lib/libinternalproj.so"

rm "/build${PROJ_INSTALL_PREFIX}/lib"/libproj.*
ln -s "libinternalproj.so.${PROJ_SO}" "/build${PROJ_INSTALL_PREFIX}/lib/libproj.so.${PROJ_SO_FIRST}"

if [ "${WITH_DEBUG_SYMBOLS}" = "yes" ]; then
    # separate debug symbols
    mkdir -p "/build${PROJ_INSTALL_PREFIX}/lib/.debug/" "/build${PROJ_INSTALL_PREFIX}/bin/.debug/"

    DEBUG_SO="/build${PROJ_INSTALL_PREFIX}/lib/.debug/libinternalproj.so.${PROJ_SO}.debug"
    objcopy -v --only-keep-debug --compress-debug-sections "${PROJ_SO_DEST}" "${DEBUG_SO}"
    strip --strip-debug --strip-unneeded "${PROJ_SO_DEST}"
    objcopy --add-gnu-debuglink="${DEBUG_SO}" "${PROJ_SO_DEST}"

    for P in "/build${PROJ_INSTALL_PREFIX}/bin"/*; do
        if file -h "$P" | grep -qi elf; then
            F=$(basename "$P")
            DEBUG_P="/build${PROJ_INSTALL_PREFIX}/bin/.debug/${F}.debug"
            objcopy -v --only-keep-debug --strip-unneeded "$P" "${DEBUG_P}"
            strip --strip-debug --strip-unneeded "$P"
            objcopy --add-gnu-debuglink="${DEBUG_P}" "$P"
        fi
    done
else
    strip -s "${PROJ_SO_DEST}"
    for P in "/build${PROJ_INSTALL_PREFIX}/bin"/*; do
        strip -s "$P" 2>/dev/null || /bin/true;
    done;
fi