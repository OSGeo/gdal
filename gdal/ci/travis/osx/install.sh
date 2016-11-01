#!/bin/sh

set -e

# build proj
brew list --versions
curl http://download.osgeo.org/proj/proj-4.9.3.tar.gz > proj-4.9.3.tar.gz
tar xvzf proj-4.9.3.tar.gz
cd proj-4.9.3/nad
curl http://download.osgeo.org/proj/proj-datumgrid-1.5.tar.gz > proj-datumgrid-1.5.tar.gz
tar xvzf proj-datumgrid-1.5.tar.gz
cd ..
./configure --prefix=$HOME/install-proj
make -j3
make install
cd ..
# build GDAL
cd gdal
CC="ccache gcc" CXX="ccache g++" ./configure --prefix=$HOME/install-gdal --enable-debug --with-jpeg12 --with-python --with-geotiff=internal --with-png=internal --with-static-proj4=$HOME/install-proj --with-sqlite3=/usr/local/opt/sqlite
make USER_DEFS="-Wextra -Werror" -j3
cd apps
make USER_DEFS="-Wextra -Werror" test_ogrsf
echo "Show which shared libs got used:"
otool -L .libs/ogrinfo
cd ..
make install
export PATH=$HOME/install-gdal/bin:$PWD/apps/.libs:$PATH
export DYLD_LIBRARY_PATH=$HOME/install-gdal/lib
export GDAL_DATA=$HOME/install-gdal/share/gdal
export PYTHONPATH=$PWD/swig/python/build/lib.macosx-10.11-x86_64-2.7
cd ../autotest/cpp
echo $PATH
#sudo rm -rf /usr/local/Cellar/gdal/1.10.1_1/*
#sudo ln -s /usr/bin /usr/local/Cellar/gdal/1.10.1_1
#sudo ln -s /usr/lib /usr/local/Cellar/gdal/1.10.1_1
#sudo ln -s /usr/include /usr/local/Cellar/gdal/1.10.1_1
#sudo mkdir /usr/local/Cellar/gdal/1.10.1_1/share
#sudo ln -s /usr/share/gdal /usr/local/Cellar/gdal/1.10.1_1/share
gdal-config --version
gdal-config --cflags
gdal-config --libs
make -j3
cd ../../gdal
