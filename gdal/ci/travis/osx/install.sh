#!/bin/sh

set -e

CONDA_PREFIX=/usr/local/miniconda/envs/test

# Few tricks from https://github.com/conda-forge/gdal-feedstock/blob/master/recipe/build.sh

# recommended in https://gitter.im/conda-forge/conda-forge.github.io?at=5c40da7f95e17b45256960ce
find ${CONDA_PREFIX}/lib -name '*.la' -delete

# build GDAL
cd gdal
./autogen.sh
# --without-tiledb because of https://github.com/OSGeo/gdal/issues/3122
./configure --prefix=$HOME/install-gdal \
    --enable-debug \
    --with-jpeg12 \
    --with-geotiff=internal \
    --with-png=internal \
    --without-pg \
    --without-jasper \
    --without-webp \
    --with-expat=${CONDA_PREFIX} \
    --with-sqlite3=${CONDA_PREFIX} \
    --with-libjson-c=${CONDA_PREFIX} \
    --without-tiledb \
    --without-python
make USER_DEFS="-Wextra -Werror" -j3
cd apps
make USER_DEFS="-Wextra -Werror" test_ogrsf
echo "Show which shared libs got used:"
otool -L .libs/ogrinfo
cd ..
cd swig/python
python3 setup.py build
cd ../..
make install
export PATH=$HOME/install-gdal/bin:$PWD/apps/.libs:$PATH
export DYLD_LIBRARY_PATH=$HOME/install-gdal/lib
export GDAL_DATA=$HOME/install-gdal/share/gdal

cd ../autotest/cpp
echo $PATH

gdal-config --version
gdal-config --cflags
gdal-config --libs
make -j3
cd ../../gdal

# Post-install testing
# ../autotest/postinstall/test_pkg-config.sh $HOME/install-gdal
