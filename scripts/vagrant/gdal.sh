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

rsync -a /vagrant/gdal/ /home/vagrant/gnumake-build-gcc4.8
rsync -a --exclude='__pycache__' /vagrant/autotest/ /home/vagrant/gnumake-build-gcc4.8/autotest
echo rsync -a /vagrant/gdal/ /home/vagrant/gnumake-build-gcc4.8/ > /home/vagrant/gnumake-build-gcc4.8/resync.sh
echo rsync -a --exclude='__pycache__'  /vagrant/autotest/ /home/vagrant/gnumake-build-gcc4.8/autotest >> /home/vagrant/gnumake-build-gcc4.8/resync.sh

chmod +x /home/vagrant/gnumake-build-gcc4.8/resync.sh
cd /home/vagrant/gnumake-build-gcc4.8

export CCACHE_CPP2=yes

ARCH_FLAGS=""
AVX2_AVAIL=1
grep avx2 /proc/cpuinfo >/dev/null || AVX2_AVAIL=0
if [[ "${AVX2_AVAIL}" == "1" ]]; then
        ARCH_FLAGS="-mavx2"
        echo "AVX2 available on CPU"
else
        echo "AVX2 not available on CPU."
        grep flags /proc/cpuinfo | head -n 1
fi

CFLAGS=$ARCH_FLAGS CXXFLAGS=$ARCH_FLAGS CC="ccache gcc" CXX="ccache g++" LDFLAGS="-lstdc++" \
./configure  --prefix=/usr --without-libtool --enable-debug --with-jpeg12 \
            --with-python --with-poppler \
            --with-podofo --with-spatialite --with-java --with-mdb \
            --with-jvm-lib-add-rpath --with-epsilon --with-gta \
            --with-mysql --with-liblzma --with-webp --with-libkml \
            --with-openjpeg=/usr/local --with-armadillo
#  --with-ecw=/usr/local --with-mrsid=/usr/local --with-mrsid-lidar=/usr/local --with-fgdb=/usr/local

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
make generate
make
make vagrant_safe_test
cd ../..

# Install pytest.
# First install pip 9, which is the last version which can upgrade the system's `six`
# 10+ throws an error :/
curl -sSL https://bootstrap.pypa.io/get-pip.py -o /tmp/get-pip.py
sudo -H python /tmp/get-pip.py 'pip<10'

sudo -H pip install -Ur /vagrant/autotest/requirements.txt

# Add python symbols so gdb is friendlier
sudo apt-get install -y python2.7-dbg
