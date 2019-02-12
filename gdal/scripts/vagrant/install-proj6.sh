#!/bin/bash

# abort install if any errors occur and enable tracing
set -o errexit

echo "Cloning proj sources..."
git clone --depth 1 https://github.com/osgeo/proj.4.git proj

echo "Build and install proj6..."
mkdir proj/build-x86_64
(cd proj; ./autogen.sh)
(cd proj/build-x86_64; ../configure --prefix=/usr/local/ && make -j3 && sudo make install )
