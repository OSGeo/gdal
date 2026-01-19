#!/bin/sh

set -e

CONDA_PREFIX=/Users/runner/miniconda3/envs/test

# Few tricks from https://github.com/conda-forge/gdal-feedstock/blob/master/recipe/build.sh

# recommended in https://gitter.im/conda-forge/conda-forge.github.io?at=5c40da7f95e17b45256960ce
find ${CONDA_PREFIX}/lib -name '*.la' -delete

# build GDAL
mkdir build
cd build

# to have an environment where we test this...
export GDAL_PYTHON_BINDINGS_WITHOUT_NUMPY=YES

CFLAGS="-Wextra -Werror" CXXFLAGS="-Wextra -Werror" cmake .. \
         -DCMAKE_INSTALL_PREFIX=${CONDA_PREFIX} \
         -DCMAKE_PREFIX_PATH=${CONDA_PREFIX} \
         -DCMAKE_BUILD_TYPE=RelWithDebInfo \
         -DGDAL_USE_GEOTIFF_INTERNAL=ON \
         -DGDAL_USE_PNG_INTERNAL=ON \
         -DGDAL_USE_POSTGRESQL=OFF \
         -DGDAL_USE_WEBP=OFF \
         -DBUILD_CSHARP_BINDINGS=OFF \
         -DGDAL_USE_KEA=OFF \
         "-DUSE_PRECOMPILED_HEADERS=ON" \
         -DCMAKE_UNITY_BUILD=ON

echo "Check that GDAL_ENABLE_ARM_NEON_OPTIMIZATIONS:BOOL=ON"
(grep "GDAL_ENABLE_ARM_NEON_OPTIMIZATIONS:BOOL=ON" CMakeCache.txt > /dev/null && echo "yes") || (echo "Missing"; clang -c ../gcore/include_sse2neon.h; clang++ -c ../gcore/include_sse2neon.h; /bin/false)

NPROC=$(sysctl -n hw.ncpu)
echo "NPROC=${NPROC}"
make -j${NPROC}

echo "Show which shared libs got used:"
otool -L apps/ogrinfo

make install

cd ..
