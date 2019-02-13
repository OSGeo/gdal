#!/bin/bash

# abort install if any errors occur and enable tracing
set -o errexit

echo "Cloning proj sources..."
git clone --depth 1 https://github.com/osgeo/proj.4.git proj

echo "Build and install proj6..."
mkdir proj/build-x86_64
(cd proj; ./autogen.sh)
(cd proj/build-x86_64; CFLAGS='-DPROJ_RENAME_SYMBOLS' CXXFLAGS='-DPROJ_RENAME_SYMBOLS' ../configure --prefix=/usr/local/ && make -j3 && sudo make install && sudo mv /usr/local/lib/libproj.so.13.1.1 /usr/local/lib/libinternalproj.so.13.1.1 && sudo rm /usr/local/lib/libproj.so* && sudo rm /usr/local/lib/libproj.a && sudo rm /usr/local/lib/libproj.la && sudo ln -s libinternalproj.so.13.1.1 /usr/local/lib/libinternalproj.so.13 && sudo ln -s libinternalproj.so.13.1.1 /usr/local/lib/libinternalproj.so)
