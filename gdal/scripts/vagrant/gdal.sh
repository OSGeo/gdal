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

cd /vagrant
#  --with-ecw=/usr/local --with-mrsid=/usr/local --with-mrsid-lidar=/usr/local --with-fgdb=/usr/local
./configure  --prefix=/usr --without-libtool --enable-debug --with-jpeg12 \
            --with-python --with-poppler \
            --with-podofo --with-spatialite --with-java --with-mdb \
            --with-jvm-lib-add-rpath --with-epsilon --with-gta \
            --with-mysql --with-liblzma --with-webp --with-libkml \
            --with-openjpeg=/usr/local --with-armadillo

make clean >/dev/null
make -j $NUMTHREADS
cd apps
make test_ogrsf
cd ..

# A previous version of GDAL has been installed by PostGIS
sudo rm -f /usr/lib/libgdal.so*
sudo make install
sudo ldconfig
# not sure why we need to do that
#sudo cp -r /usr/lib/python2.7/site-packages/*  /usr/lib/python2.7/dist-packages/

cd swig/perl
make veryclean
make
make test
cd ../..

cd swig/java
JAVA_HOME=/usr/lib/jvm/java-7-openjdk-amd64 make
make test
cd ../..

cd swig/csharp
# There's an issue with swig 2.0.4 from ubuntu 12.04
PATH=$HOME/install-swig-1.3.40/bin:$PATH make generate
make
# For some reason, this fails on Vagrant ubuntu 12.04
# make test
cd ../..
