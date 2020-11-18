#!/bin/sh

set -e

dpkg --add-architecture i386
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

sudo apt-get install -y \
    ccache \
    binutils-mingw-w64-x86-64 \
    gcc-mingw-w64-x86-64 \
    g++-mingw-w64-x86-64 \
    g++-mingw-w64 \
    mingw-w64-tools \
    wine1.4-amd64 \
    make sqlite3 \
    curl


export CCACHE_CPP2=yes
export CC="ccache x86_64-w64-mingw32-gcc"
export CXX="ccache x86_64-w64-mingw32-g++"

ccache -M 1G
ccache -s

wine64 cmd /c dir
ln -s /usr/lib/gcc/x86_64-w64-mingw32/4.8/libstdc++-6.dll  $HOME/.wine/drive_c/windows
ln -s /usr/lib/gcc/x86_64-w64-mingw32/4.8/libgcc_s_sjlj-1.dll  $HOME/.wine/drive_c/windows
ln -s /usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll  $HOME/.wine/drive_c/windows

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
$SCRIPT_DIR/../common_install.sh

# build sqlite3
wget https://sqlite.org/2018/sqlite-autoconf-3250100.tar.gz
tar xzf sqlite-autoconf-3250100.tar.gz
(cd sqlite-autoconf-3250100 && ./configure  --host=x86_64-w64-mingw32 --prefix=/tmp/install && make -j3 && make install)

# Build proj
(cd proj; ./autogen.sh && CFLAGS='-DPROJ_RENAME_SYMBOLS' CXXFLAGS='-DPROJ_RENAME_SYMBOLS' SQLITE3_CFLAGS='-I/tmp/install/include' SQLITE3_LIBS='-L/tmp/install/lib -lsqlite3' ./configure --disable-static --host=x86_64-w64-mingw32 --prefix=/tmp/install && make -j3)
(cd proj; sudo make -j3 install)

# build GDAL
cd gdal
./configure --host=x86_64-w64-mingw32 --with-proj=/tmp/install
make USER_DEFS="-Wextra -Werror" -j3
cd apps
make USER_DEFS="-Wextra -Werror" test_ogrsf.exe
cd ..
ln -sf $PWD/.libs/libgdal-*.dll $HOME/.wine/drive_c/windows
ln -sf /tmp/install/bin/libproj-15.dll $HOME/.wine/drive_c/windows
ln -sf /tmp/install/bin/libsqlite3-0.dll $HOME/.wine/drive_c/windows
# Python bindings
wget https://www.python.org/ftp/python/2.7.15/python-2.7.15.amd64.msi
wine64 msiexec /i python-2.7.15.amd64.msi
cd swig/python
gendef $HOME/.wine/drive_c/Python27/python27.dll
x86_64-w64-mingw32-dlltool --dllname $HOME/.wine/drive_c/Python27/python27.dll --input-def python27.def --output-lib $HOME/.wine/drive_c/Python27/libs/libpython27.a
CXX=x86_64-w64-mingw32-g++ bash fallback_build_mingw32_under_unix.sh 
cd ../..

ccache -s

echo "Saving ccache..."
rm -f "$WORK_DIR/ccache.tar.gz"
(cd $HOME && tar czf "$WORK_DIR/ccache.tar.gz" .ccache)


wine64 apps/gdalinfo.exe --version
cd ../autotest
# Does not work under wine
rm gcore/rfc30.py
rm gnm/gnm_test.py

# For some reason this crashes in the matrix .travis.yml but not in standalone branch
rm pyscripts/test_gdal2tiles.py

export PYTHON_DIR="$HOME/.wine/drive_c/Python27"

# install test dependencies
curl https://bootstrap.pypa.io/get-pip.py -o get-pip.py
wine64 "$PYTHON_DIR/python.exe" get-pip.py
rm get-pip.py
# running pip2.7.exe doesn't seem to work in wine. workaround is use `-m pip`
# https://forum.winehq.org/viewtopic.php?f=2&t=22522
wine64 "$PYTHON_DIR/python.exe" -m pip install -U -r ./requirements.txt
# same issue with running pytest.exe
export PYTEST="wine64 $PYTHON_DIR/python.exe -m pytest -vv -p no:sugar --color=no"


# Run all the Python autotests
GDAL_DATA=$PWD/../gdal/data \
    PYTHONPATH=$PWD/../gdal/swig/python/build/lib.win-amd64-2.7 \
    PATH=$PWD/../gdal:$PWD/../gdal/apps/.libs:$PWD:$PATH \
    $PYTEST
