#!/bin/sh

set -e

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
./autogen.sh
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
