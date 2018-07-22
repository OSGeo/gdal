#!/bin/bash

# abort install if any errors occur and enable tracing
set -o errexit
set -o xtrace

git clone https://github.com/libkml/libkml.git libkml
mkdir libkml/build
cd libkml/build
cmake -G Ninja -DCMAKE_INSTALL_PREFIX=/usr/local ..
cmake --build .
sudo cmake --build . --target install

cd ../..
