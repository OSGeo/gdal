#!/bin/bash

# abort install if any errors occur and enable tracing
set -o errexit
set -o xtrace

NUMTHREADS=2
if [[ -f /sys/devices/system/cpu/online ]]; then
	# Calculates 1.5 times physical threads
	NUMTHREADS=$(( ( $(cut -f 2 -d '-' /sys/devices/system/cpu/online) + 1 ) * 15 / 10  ))
fi
#NUMTHREADS=1 # disable MP
export NUMTHREADS

git clone https://github.com/libkml/libkml.git libkml
mkdir libkml/build
cd libkml/build
cmake -G Ninja -DCMAKE_INSTALL_PREFIX=/usr/local ..
cmake --build .
sudo cmake --build . --target install

cd ../..
