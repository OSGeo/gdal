#!/bin/sh

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

wget http://sourceforge.net/projects/openjpeg.mirror/files/openjpeg-2.0.0.tar.gz/download -O openjpeg-2.0.0.tar.gz
tar xvzf openjpeg-2.0.0.tar.gz
cd openjpeg-2.0.0
mkdir build
cd build
cmake ..

make -j $NUMTHREADS
sudo make install

cd ../..
