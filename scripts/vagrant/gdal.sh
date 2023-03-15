#!/bin/bash

# abort install if any errors occur and enable tracing
set -o errexit
set -o xtrace

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

export CCACHE_DIR=/vagrant/ccache_vagrant

cd /vagrant
mkdir -p build_vagrant
cd build_vagrant
export CFLAGS="$ARCH_FLAGS -Werror"
export CXXFLAGS="$ARCH_FLAGS -Werror"
cmake .. \
  -GNinja \
  -DCMAKE_INSTALL_PREFIX=/opt/gdal-dev \
  -DUSE_CCACHE=ON \
  -DUSE_ALTERNATE_LINKER:STRING=mold \
  -DCMAKE_BUILD_TYPE=Debug
ninja -j6
sudo ninja install

python3 -m pip install -r ../autotest/requirements.txt
