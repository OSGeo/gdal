#!/bin/sh

set -e

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

# Emulate 'mingw_w64' Travis-CI target for the purpose of test skipping
TRAVIS=yes
export TRAVIS
TRAVIS_BRANCH=mingw_w64
export TRAVIS_BRANCH

apt-get update -y
DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    sudo wget tzdata

USER=root
export USER

cd "$WORK_DIR"

if test -f "$WORK_DIR/ccache.tar.gz"; then
    echo "Restoring ccache..."
    (cd $HOME && tar xzf "$WORK_DIR/ccache.tar.gz")
fi

# Install python
sh $SCRIPT_DIR/install-python.sh
export WINEPREFIX=$HOME/.wine64

sudo apt-get install -y --no-install-recommends \
    ccache automake cmake \
    binutils-mingw-w64-x86-64 \
    gcc-mingw-w64-x86-64 \
    g++-mingw-w64-x86-64 \
    g++-mingw-w64 \
    mingw-w64-tools \
    make sqlite3 \
    curl

export CCACHE_CPP2=yes
export CC="ccache x86_64-w64-mingw32-gcc"
export CXX="ccache x86_64-w64-mingw32-g++"

# Select posix/pthread for std::mutex
update-alternatives --set x86_64-w64-mingw32-gcc /usr/bin/x86_64-w64-mingw32-gcc-posix
update-alternatives --set x86_64-w64-mingw32-g++ /usr/bin/x86_64-w64-mingw32-g++-posix

ccache -M 1G
ccache -s

ln -sf /usr/lib/gcc/x86_64-w64-mingw32/7.3-posix/libstdc++-6.dll $WINEPREFIX/drive_c/windows/
ln -sf /usr/lib/gcc/x86_64-w64-mingw32/7.3-posix/libgcc_s_seh-1.dll $WINEPREFIX/drive_c/windows/
ln -sf /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll $WINEPREFIX/drive_c/windows/

$SCRIPT_DIR/../common_install.sh

# build sqlite3
wget https://sqlite.org/2018/sqlite-autoconf-3250100.tar.gz
tar xzf sqlite-autoconf-3250100.tar.gz
(cd sqlite-autoconf-3250100 && CFLAGS="-O2 -DSQLITE_ENABLE_COLUMN_METADATA" ./configure  --host=x86_64-w64-mingw32 --enable-rtree --prefix=/tmp/install && make -j3 && make install)

# Build proj
(cd proj; ./autogen.sh && CFLAGS='-DPROJ_RENAME_SYMBOLS' CXXFLAGS='-DPROJ_RENAME_SYMBOLS' SQLITE3_CFLAGS='-I/tmp/install/include' SQLITE3_LIBS='-L/tmp/install/lib -lsqlite3' ./configure --disable-static --host=x86_64-w64-mingw32 --prefix=/tmp/install && make -j3)
(cd proj; sudo make -j3 install)

# build GDAL
mkdir build
cd build
cmake .. -DPROJ_INCLUDE_DIR=/tmp/install/include -DPROJ_LIBRARY=/tmp/install/lib/libproj.dll.a -DSQLITE3_INCLUDE_DIR=/tmp/install/include -DSQLITE3_LIBRARY=/tmp/install/lib/libsqlite3.dll.a -DCMAKE_C_FLAGS="-DPROJ_RENAME_SYMBOLS -Werror" -DCMAKE_CXX_FLAGS="-DPROJ_RENAME_SYMBOLS -Werror" -DBUILD_PYTHON_BINDINGS=OFF -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_SYSTEM_VERSION=1 -DCMAKE_C_COMPILER=x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=x86_64-w64-mingw32-g++ -DCMAKE_RC_COMPILER=86_64-w64-mingw32-windres -DCMAKE_RANLIB=x86_64-w64-mingw32-ranlib -DCMAKE_FIND_ROOT_PATH=/usr/x86_64-w64-mingw32 -DCMAKE_FIND_ROOT_PATH_MODE_PROGRAM=NEVER -DCMAKE_FIND_ROOT_PATH_MODE_LIBRARY=ONLY -DCMAKE_FIND_ROOT_PATH_MODE_INCLUDE=ONLY

make -j3
ln -sf $PWD/libgdal-*.dll $WINEPREFIX/drive_c/windows
ln -sf /tmp/install/bin/libproj-15.dll $WINEPREFIX/drive_c/windows
ln -sf /tmp/install/bin/libsqlite3-0.dll $WINEPREFIX/drive_c/windows
cd ..

cd swig/python
ln -s "$WINEPREFIX/drive_c/users/root/Local Settings/Application Data/Programs/Python/Python37" $WINEPREFIX/drive_c/Python37
gendef $WINEPREFIX/drive_c/Python37/python37.dll
x86_64-w64-mingw32-dlltool --dllname $WINEPREFIX/drive_c/Python37/python37.dll --input-def python37.def --output-lib $WINEPREFIX/drive_c/Python37/libs/libpython37.a
bash fallback_build_mingw32_under_unix_py37.sh
cd ../..

ccache -s

echo "Saving ccache..."
rm -f "$WORK_DIR/ccache.tar.gz"
(cd $HOME && tar czf "$WORK_DIR/ccache.tar.gz" .ccache)


wine64 build/apps/gdalinfo.exe --version
rm -f swig/python/gdal-utils/osgeo_utils/samples/validate_gpkg.py  # the sqlite3 lib of python lacks the rtree module
cd autotest
# Does not work under wine
rm -f gcore/rfc30.py
rm -f pyscripts/data/test_utf8*
rm -rf pyscripts/data/漢字

export PYTHON_DIR="$WINEPREFIX/drive_c/Python37"

# install test dependencies
wine64 "$PYTHON_DIR/Scripts/pip.exe" install -U -r ./requirements.txt

export PYTEST="wine64 $PYTHON_DIR/python.exe -m pytest -vv -p no:sugar --color=no"


# Run all the Python autotests
GDAL_DATA=$PWD/../data \
    PYTHONPATH=$PWD/../swig/python/build/lib.win-amd64-3.7 \
    PATH=$PWD/../gdal/build:$PWD/../build/apps:$PWD:$PATH \
    $PYTEST
