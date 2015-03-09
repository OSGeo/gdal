#!/bin/sh

NUMTHREADS=2
if [[ -f /sys/devices/system/cpu/online ]]; then
        # Calculates 1.5 times physical threads
        NUMTHREADS=$(( ( $(cut -f 2 -d '-' /sys/devices/system/cpu/online) + 1 ) * 15 / 10  ))
fi
#NUMTHREADS=1 # disable MP
export NUMTHREADS

wget "http://downloads.sourceforge.net/project/swig/swig/swig-1.3.40/swig-1.3.40.tar.gz?r=http%3A%2F%2Fsourceforge.net%2Fprojects%2Fswig%2Ffiles%2Fswig%2Fswig-1.3.40%2F&ts=1425900154&use_mirror=freefr" -O swig-1.3.40.tar.gz
tar xvzf swig-1.3.40.tar.gz
cd  swig-1.3.40
./configure --prefix=$HOME/install-swig-1.3.40
make -j $NUMTHREADS
make install

