#!/bin/bash

# abort install if any errors occur and enable tracing
set -o errexit

SQLITE_YEAR=2019
SQLITE_VER=3270100

echo "Download, build and install sqlite3 for mingw"
wget -nc -c -P /var/cache/wget https://sqlite.org/${SQLITE_YEAR}/sqlite-autoconf-${SQLITE_VER}.tar.gz
tar xf /var/cache/wget/sqlite-autoconf-${SQLITE_VER}.tar.gz
mkdir sqlite-autoconf-${SQLITE_VER}/build-mingw-w64
(cd sqlite-autoconf-${SQLITE_VER}/build-mingw-w64; ../configure CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++ --build=x86_64-w64-mingw32 --prefix=/usr/local/x86_64-w64-mingw32/ --with-sysroot=/usr/x86_64-w64-mingw32/ && make -j3 && sudo make install )

echo "Build and install proj6 for mingw"
mkdir proj/build-mingw-w64
(cd proj/build-mingw-w64; ../configure SQLITE3_CFLAGS="-I/usr/local/x86_64-w64-mingw32/include" SQLITE3_LIBS="-L/usr/local/x86_64-w64-mingw32/lib -lsqlite3"  CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++ --host=x86_64-w64-mingw32 --prefix=/usr/local/x86_64-w64-mingw32/ --with-sysroot=/usr/x86_64-w64-mingw32/ && make -j3 && sudo make install )
