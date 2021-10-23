#!/bin/sh

set -e

apt-get update -y

# python needed for make-standalone-toolchain.sh
# pkg-config sqlite3 for proj compilation
# libncurses5 since android clang links against it
DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    wget unzip ccache curl ca-certificates \
    python \
    pkg-config sqlite3 \
    automake \
    libncurses5

cd "$WORK_DIR"

if test -f "$WORK_DIR/ccache.tar.gz"; then
    echo "Restoring ccache..."
    (cd $HOME && tar xzf "$WORK_DIR/ccache.tar.gz")
fi

wget -q https://dl.google.com/android/repository/android-ndk-r16b-linux-x86_64.zip
unzip -q android-ndk-r16b-linux-x86_64.zip
android-ndk-r16b/build/tools/make-standalone-toolchain.sh --platform=android-24 --install-dir=$HOME/android-toolchain --stl=libc++ --verbose

export PATH=$HOME/android-toolchain/bin:$PATH
export CC="ccache arm-linux-androideabi-clang"
export CXX="ccache arm-linux-androideabi-clang++"
export CFLAGS="-mthumb -Qunused-arguments"
export CXXFLAGS="-mthumb -Qunused-arguments"

ccache -M 1G
ccache -s

SCRIPT_DIR=$(dirname "$0")
case $SCRIPT_DIR in
    "/"*)
        ;;
    ".")
        SCRIPT_DIR=$(pwd)
        ;;
    *)
        SCRIPT_DIR=$(pwd)/$(dirname "$0")
        ;;
esac
"$SCRIPT_DIR"/../common_install.sh

# build sqlite3
wget -q https://sqlite.org/2018/sqlite-autoconf-3250100.tar.gz
tar xzf sqlite-autoconf-3250100.tar.gz
(cd sqlite-autoconf-3250100 && ./configure --host=arm-linux-androideabi --prefix=/tmp/install && make -j3 && make install)

# Build proj
(cd proj;  ./autogen.sh && PKG_CONFIG_PATH=/tmp/install/lib/pkgconfig ./configure --host=arm-linux-androideabi --prefix=/tmp/install --disable-static && make -j3 && make install)

./autogen.sh
./configure --host=arm-linux-androideabi --with-proj=/tmp/install --with-sqlite3=/tmp/install
make USER_DEFS="-Wextra -Werror" -j3

ccache -s

echo "Saving ccache..."
rm -f "$WORK_DIR/ccache.tar.gz"
(cd $HOME && tar czf "$WORK_DIR/ccache.tar.gz" .ccache)
