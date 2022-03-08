#!/bin/sh

set -e

apt-get update -y

# pkg-config sqlite3 for proj compilation
DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    wget unzip ccache curl ca-certificates \
    pkg-config make binutils sqlite3 \
    automake

cd "$WORK_DIR"

if test -f "$WORK_DIR/ccache.tar.gz"; then
    echo "Restoring ccache..."
    (cd $HOME && tar xzf "$WORK_DIR/ccache.tar.gz")
fi

# We need a recent cmake for recent NDK versions
wget -q https://github.com/Kitware/CMake/releases/download/v3.22.3/cmake-3.22.3-linux-x86_64.tar.gz
tar xzf cmake-3.22.3-linux-x86_64.tar.gz
export PATH=$PWD/cmake-3.22.3-linux-x86_64/bin:$PATH

# Download Android NDK
wget -q https://dl.google.com/android/repository/android-ndk-r23b-linux.zip
unzip -q android-ndk-r23b-linux.zip

export ANDROID_NDK=$PWD/android-ndk-r23b
export NDK_TOOLCHAIN=$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64

ccache -M 1G
ccache -s

# build sqlite3
wget -q https://sqlite.org/2022/sqlite-autoconf-3370200.tar.gz
tar xzf sqlite-autoconf-3370200.tar.gz
cd sqlite-autoconf-3370200
CC="ccache $NDK_TOOLCHAIN/bin/aarch64-linux-android24-clang" ./configure \
  --prefix=/tmp/install --host=x86_64-linux-android24
make -j3
make install
cd ..

# Build proj
wget -q https://download.osgeo.org/proj/proj-9.0.0.tar.gz
tar xzf proj-9.0.0.tar.gz
cd proj-9.0.0
mkdir build
cd build
# See later comment in GDAL build section about MAKE_FIND_ROOT_PATH_MODE_INCLUDE, CMAKE_FIND_ROOT_PATH_MODE_LIBRARY
cmake .. \
  -DUSE_CCACHE=ON \
  -DENABLE_TIFF=OFF -DENABLE_CURL=OFF -DBUILD_APPS=OFF -DBUILD_TESTING=OFF \
  -DCMAKE_INSTALL_PREFIX=/tmp/install \
  -DCMAKE_SYSTEM_NAME=Android \
  -DCMAKE_ANDROID_NDK=$ANDROID_NDK \
  -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a \
  -DCMAKE_SYSTEM_VERSION=24 \
  "-DCMAKE_PREFIX_PATH=/tmp/install;$NDK_TOOLCHAIN/sysroot/usr/" \
  -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=NEVER \
  -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=NEVER \
  -DEXE_SQLITE3=/usr/bin/sqlite3
make -j3
make install
cd ../..

# Build GDAL
mkdir build_android_cmake
cd build_android_cmake

# PKG_CONFIG_LIBDIR, CMAKE_FIND_ROOT_PATH_MODE_INCLUDE, CMAKE_FIND_ROOT_PATH_MODE_LIBRARY, CMAKE_FIND_USE_CMAKE_SYSTEM_PATH
# are needed because we don't install dependencies (PROJ, SQLite3) in the NDK sysroot
# This is definitely not the most idiomatic way of proceeding...
PKG_CONFIG_LIBDIR=/tmp/install/lib/pkgconfig cmake .. \
 -DUSE_CCACHE=ON \
 -DCMAKE_INSTALL_PREFIX=/tmp/install \
 -DCMAKE_SYSTEM_NAME=Android \
 -DCMAKE_ANDROID_NDK=$ANDROID_NDK \
 -DCMAKE_ANDROID_ARCH_ABI=arm64-v8a \
 -DCMAKE_SYSTEM_VERSION=24 \
 "-DCMAKE_PREFIX_PATH=/tmp/install;$NDK_TOOLCHAIN/sysroot/usr/" \
 -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=NEVER \
 -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=NEVER \
 -DCMAKE_FIND_USE_CMAKE_SYSTEM_PATH=NO \
 -DSFCGAL_CONFIG=disabled \
 -DHDF5_C_COMPILER_EXECUTABLE=disabled \
 -DHDF5_CXX_COMPILER_EXECUTABLE=disabled
make -j3
make install
cd ..

ccache -s

echo "Saving ccache..."
rm -f "$WORK_DIR/ccache.tar.gz"
(cd $HOME && tar czf "$WORK_DIR/ccache.tar.gz" .ccache)
