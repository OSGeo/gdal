#!/bin/sh

set -e

CONDA_PREFIX=/usr/local/miniconda/envs/test

# Few tricks from https://github.com/conda-forge/gdal-feedstock/blob/master/recipe/build.sh

# recommended in https://gitter.im/conda-forge/conda-forge.github.io?at=5c40da7f95e17b45256960ce
find ${CONDA_PREFIX}/lib -name '*.la' -delete

# build GDAL
mkdir build
cd build
# Disable Arrow/Parquet because the VM provides libraries in /usr/local/lib/
# that cause Illegal instruction error when running tests. I suspect the
# Arrow/Parquet libraries to be built with AVX2 support but the VM worker
# doesn't support it.
CFLAGS="-Wextra -Werror" CXXFLAGS="-Wextra -Werror" cmake .. \
         -DCMAKE_INSTALL_PREFIX=$HOME/install-gdal \
         -DCMAKE_PREFIX_PATH=${CONDA_PREFIX} \
         -DCMAKE_BUILD_TYPE=Debug \
         -DGDAL_USE_GEOTIFF_INTERNAL=ON \
         -DGDAL_USE_PNG_INTERNAL=ON \
         -DGDAL_USE_POSTGRESQL=OFF \
         -DGDAL_USE_WEBP=OFF \
         -DGDAL_USE_ARROW=OFF \
         -DGDAL_USE_PARQUET=OFF \
         -DBUILD_CSHARP_BINDINGS=OFF \
         -DCMAKE_UNITY_BUILD=ON
make -j3
echo "Show which shared libs got used:"
otool -L apps/ogrinfo
make install
cd ..

# Post-install testing
# ../autotest/postinstall/test_pkg-config.sh $HOME/install-gdal
