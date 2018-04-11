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

wget --no-check-certificate https://sourceforge.net/projects/swig/files/swig/swig-1.3.40/swig-1.3.40.tar.gz/download -O swig-1.3.40.tar.gz
tar xvzf swig-1.3.40.tar.gz
cd  swig-1.3.40
./configure --prefix="$HOME/install-swig-1.3.40"
make -j $NUMTHREADS
make install

