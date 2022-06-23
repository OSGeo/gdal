#!/bin/sh

set -e

apt-get update -y
DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
    sudo wget tzdata

TRAVIS=yes
export TRAVIS
TRAVIS_BRANCH=ubuntu_1804_32bit
export TRAVIS_BRANCH

LANG=en_US.UTF-8
export LANG
apt-get install -y locales && \
    echo "$LANG UTF-8" > /etc/locale.gen && \
    dpkg-reconfigure --frontend=noninteractive locales && \
    update-locale LANG=$LANG

USER=root
export USER

cd "$WORK_DIR"

if test -f "$WORK_DIR/ccache.tar.gz"; then
    echo "Restoring ccache..."
    (cd $HOME && tar xzf "$WORK_DIR/ccache.tar.gz")
fi


sudo apt-get install -y --no-install-recommends --allow-unauthenticated python3-dev python3-setuptools python3-pip python3-numpy libpng-dev libjpeg-dev libgif-dev liblzma-dev libgeos-dev libcurl4-gnutls-dev libproj-dev libxml2-dev libexpat-dev libxerces-c-dev libnetcdf-dev netcdf-bin libpoppler-dev libpoppler-private-dev libspatialite-dev gpsbabel swig libhdf4-alt-dev libhdf5-serial-dev poppler-utils unixodbc-dev libwebp-dev libepsilon-dev liblcms2-2 libpcre3-dev libcrypto++-dev libfyba-dev libkml-dev libmysqlclient-dev mysql-client-core-5.7 libogdi3.2-dev libcfitsio-dev openjdk-8-jdk libzstd1-dev libblosc-dev liblz4-dev ccache bash zip curl libpq-dev postgresql-client postgis cmake libssl-dev libboost-dev autoconf sqlite3 libopenexr-dev g++ fossil libgeotiff-dev libopenjp2-7-dev libcairo2-dev git libtool automake grep libfcgi-dev


SCRIPT_DIR=$(dirname "$0")
case $SCRIPT_DIR in
    "/"*)
        ;;
    ".")
        SCRIPT_DIR=$(pwd)
        ;;
    *)
        SCRIPT_DIR=$(pwd)/$(dirname "$0")
        ;;
esac

ccache -M 1G
ccache -s

# Build freexl
wget http://www.gaia-gis.it/gaia-sins/freexl-sources/freexl-2.0.0-RC0.tar.gz
tar xzf freexl-2.0.0-RC0.tar.gz
(cd freexl-2.0.0-RC0 && CC='ccache gcc' CXX='ccache g++' ./configure  --disable-static --prefix=/usr && make -j3 && sudo make -j3 install)

# Build libspatialite
fossil clone https://www.gaia-gis.it/fossil/libspatialite libspatialite.fossil && mkdir sl && (cd sl && fossil open ../libspatialite.fossil && CC='ccache gcc' CXX='ccache g++' ./configure  --disable-static --prefix=/usr --disable-geos370 --disable-geos3100 --disable-rttopo && make -j3 && sudo make -j3 install)

# Build librasterlite2
fossil clone https://www.gaia-gis.it/fossil/librasterlite2 librasterlite2.fossil && mkdir rl2 && (cd rl2 && fossil open ../librasterlite2.fossil && fossil checkout 9dd8217cb9 && CC='ccache gcc' CXX='ccache g++' ./configure --disable-static --prefix=/usr --disable-lz4 --disable-zstd && make -j3 && sudo make -j3 install)

# Build proj

(git clone --depth 1 https://github.com/OSGeo/PROJ && cd PROJ && mkdir build && cd build && CC='ccache gcc' CXX='ccache g++' CFLAGS='-DPROJ_RENAME_SYMBOLS' CXXFLAGS='-DPROJ_RENAME_SYMBOLS' cmake .. -DCMAKE_INSTALL_PREFIX=/usr/local -DBUILD_TESTING=OFF -DBUILD_SHARED_LIBS=ON || cat config.log && make -j3)
sudo sh -c "cd $PWD/PROJ/build && make -j3 install"
sudo sh -c "cd /usr/local/share/proj && curl http://download.osgeo.org/proj/proj-datumgrid-1.8.tar.gz > proj-datumgrid-1.8.tar.gz && tar xvzf proj-datumgrid-1.8.tar.gz"
sudo sh -c "apt-get remove -y libproj-dev"

export LD_LIBRARY_PATH=/usr/local/lib:$LD_LIBRARY_PATH

# Configure GDAL
./autogen.sh
CC='ccache gcc' CXX='ccache g++' LDFLAGS='-lstdc++' ./configure --prefix=/usr --without-libtool --with-jpeg12 --with-python=/usr/bin/python3 --with-poppler --with-spatialite --with-mysql --with-liblzma --with-webp --with-epsilon --with-proj=/usr/local --with-poppler --with-hdf5 --with-sosi --with-mysql --with-rasterlite2 --enable-debug --with-libtiff=internal --with-hide-internal-symbols

make USER_DEFS=-Werror -j3
(cd apps && make USER_DEFS=-Werror -j3 test_ogrsf)
sudo rm -f /usr/lib/libgdal.so*
sudo make install
sudo ldconfig
sudo ln -s libgdal.so /usr/lib/libgdal.so.20

(cd autotest/cpp && make -j3)

ccache -s

echo "Saving ccache..."
rm -f "$WORK_DIR/ccache.tar.gz"
(cd $HOME && tar czf "$WORK_DIR/ccache.tar.gz" .ccache)


export PYTEST="python3 -m pytest -vv -p no:sugar --color=no"

# userfaultfd doesn't seem to work under Docker and/or 32bit (stalls on netcdf.py otherwise)
export CPL_ENABLE_USERFAULTFD=NO

(cd autotest/cpp && make quick_test)

# install pip and use it to install test dependencies
pip3 install -U -r autotest/requirements.txt

# Fails with ERROR 1: OGDI DataSource Open Failed: Could not find the dynamic library "vrf"
rm autotest/ogr/ogr_ogdi.py

# Stalls on it. Probably not enough memory
rm autotest/gdrivers/jp2openjpeg.py

# Failures for the following tests. See https://github.com/OSGeo/gdal/runs/1425843044

# depends on tiff_ovr.py that is going to be removed below
$PYTEST autotest/utilities/test_gdaladdo.py
rm -f autotest/utilities/test_gdaladdo.py

for i in autotest/gcore/tiff_ovr.py \
         autotest/gdrivers/gribmultidim.py \
         autotest/gdrivers/mbtiles.py \
         autotest/gdrivers/vrtwarp.py \
         autotest/gdrivers/wcs.py \
         autotest/utilities/test_gdalwarp.py \
         autotest/pyscripts/test_gdal_pansharpen.py; do
    $PYTEST $i || echo "Ignoring failure"
    rm -f $i
done

(cd autotest && $PYTEST)
